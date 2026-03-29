#include "topoSymGPUDeformer.h"
#include "topoSymDeformer.h"

#include <maya/MGlobal.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPxDeformerNode.h>

#include <CL/cl.h>

#include <cstring>
#include <vector>

// ── Embedded OpenCL kernel source ────────────────────────────────────────────
//
// Design notes
// ────────────
// • One work-item per vertex  → maximum GPU parallelism.
// • Positions are stored as a flat float array [x0,y0,z0, x1,y1,z1, …].
//   We use explicit index arithmetic (vid*3 + component) instead of float3[]
//   so that strides always match Maya's 12-byte layout, avoiding the
//   float3-alignment pitfall (OpenCL float3 is padded to 16 bytes).
// • The kernel reads from `input` and writes to `output`, which are SEPARATE
//   buffers – no in-place aliasing issues.
// • symTable and weights are read-only buffers uploaded ONCE and reused every
//   frame (the critical optimisation over a naïve per-frame upload approach).
// • Early-exit branches are ordered from most-likely to least-likely to
//   minimise warp divergence.

const char* TopoSymGPUDeformer::s_kernelSource = R"CL(
__kernel void topoSymDeform(
    __global const float* input,      // input positions:  numVerts * 3 floats
    __global       float* output,     // output positions: numVerts * 3 floats
    __global const int*   symTable,   // symTable[i] = source vertex, or -1
    __global const float* weights,    // per-vertex deformer weight [0,1]
    const float  envelope,            // global envelope
    const int    numVertices,
    const int    mirrorAxis           // 0=X  1=Y  2=Z
)
{
    const int vid = get_global_id(0);
    if (vid >= numVertices) return;

    const int base = vid * 3;

    // Default: pass input straight to output
    output[base    ] = input[base    ];
    output[base + 1] = input[base + 1];
    output[base + 2] = input[base + 2];

    // Check symmetry mapping
    const int srcIdx = symTable[vid];
    if (srcIdx < 0 || srcIdx == vid) return;

    // Effective weight
    const float w = weights[vid] * envelope;
    if (w <= 0.0f) return;

    // Read source position
    const int srcBase = srcIdx * 3;
    float sx = input[srcBase    ];
    float sy = input[srcBase + 1];
    float sz = input[srcBase + 2];

    // Mirror across the chosen axis
    if      (mirrorAxis == 0) { sx = -sx; }
    else if (mirrorAxis == 1) { sy = -sy; }
    else                      { sz = -sz; }

    // Linear blend: out = in * (1-w) + mirrored * w
    const float iw = 1.0f - w;
    output[base    ] = input[base    ] * iw + sx * w;
    output[base + 1] = input[base + 1] * iw + sy * w;
    output[base + 2] = input[base + 2] * iw + sz * w;
}
)CL";

// ── Constructor / Destructor ─────────────────────────────────────────────────

TopoSymGPUDeformer::TopoSymGPUDeformer()  = default;
TopoSymGPUDeformer::~TopoSymGPUDeformer() = default;

// ── creator ──────────────────────────────────────────────────────────────────

MPxGPUDeformer* TopoSymGPUDeformer::creator()
{
    return new TopoSymGPUDeformer();
}

// ── getGPUDeformerInfo ───────────────────────────────────────────────────────

MGPUDeformerRegistrationInfo* TopoSymGPUDeformer::getGPUDeformerInfo()
{
    static TopoSymGPUDeformerInfo info;
    return &info;
}

// ── terminate ────────────────────────────────────────────────────────────────
//
// Called by Maya when the deformer is destroyed or the GPU deformer is
// disabled.  Release all GPU resources so that cl_mem objects are freed
// before the OpenCL context is torn down.

void TopoSymGPUDeformer::terminate()
{
    m_kernel.reset();
    m_symTableBuffer.reset();
    m_weightsBuffer.reset();
    m_cachedNumVerts = 0;
    m_symTableDirty  = true;
    m_weightsDirty   = true;
}

// ── initKernel ───────────────────────────────────────────────────────────────
//
// Compile the OpenCL program from s_kernelSource and extract the kernel.
// This is called exactly once; subsequent calls to evaluate() skip it because
// m_kernel.isValid() will be true.

MStatus TopoSymGPUDeformer::initKernel()
{
    cl_int       err    = CL_SUCCESS;
    cl_context   ctx    = MOpenCLInfo::getMayaDefaultOpenCLContextId();
    cl_device_id device = MOpenCLInfo::getDevice();

    const char* src    = s_kernelSource;
    const size_t srcLen = std::strlen(src);

    cl_program program = clCreateProgramWithSource(ctx, 1, &src, &srcLen, &err);
    if (err != CL_SUCCESS)
    {
        MGlobal::displayError("TopoSymGPUDeformer: clCreateProgramWithSource failed.");
        return MS::kFailure;
    }

    err = clBuildProgram(program, 1, &device,
                         "-cl-fast-relaxed-math",  // allow faster FP (safe here)
                         nullptr, nullptr);
    if (err != CL_SUCCESS)
    {
        // Capture and display the build log for diagnostics
        size_t logSize = 0;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                              0, nullptr, &logSize);
        std::vector<char> log(logSize + 1, '\0');
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                              logSize, log.data(), nullptr);
        MGlobal::displayError(
            MString("TopoSymGPUDeformer: kernel build failed:\n") + log.data());
        clReleaseProgram(program);
        return MS::kFailure;
    }

    cl_kernel kernel = clCreateKernel(program, "topoSymDeform", &err);
    clReleaseProgram(program);   // program ref no longer needed after kernel is created
    if (err != CL_SUCCESS)
    {
        MGlobal::displayError("TopoSymGPUDeformer: clCreateKernel failed.");
        return MS::kFailure;
    }

    m_kernel = MAutoCLKernel(kernel);
    return MS::kSuccess;
}

// ── uploadSymTableBuffer ─────────────────────────────────────────────────────
//
// Upload the symmetry table to a GPU buffer.
// If the vertex count has not changed the existing buffer is reused (write
// only the data, no reallocation).  This path is taken when the user re-paints
// weights without changing topology.
//
// If the vertex count changed, the old buffer is released and a new one is
// created.

MStatus TopoSymGPUDeformer::uploadSymTableBuffer(const MIntArray& symTable)
{
    const unsigned int n = symTable.length();
    if (n == 0)
        return MS::kFailure;

    cl_int     err = CL_SUCCESS;
    cl_context ctx = MOpenCLInfo::getMayaDefaultOpenCLContextId();

    // Flatten MIntArray to a contiguous C array
    std::vector<cl_int> data(n);
    for (unsigned int i = 0; i < n; ++i)
        data[i] = symTable[i];

    const size_t bufSize = n * sizeof(cl_int);

    if (m_cachedNumVerts != n)
    {
        // Vertex count changed – release old buffers and allocate fresh ones
        m_symTableBuffer.reset();
        m_weightsBuffer.reset();   // weights buffer size is also stale
        m_cachedNumVerts = n;

        cl_mem buf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                    bufSize, data.data(), &err);
        if (err != CL_SUCCESS)
            return MS::kFailure;
        m_symTableBuffer = MAutoCLMem(buf);
    }
    else
    {
        // Same vertex count – overwrite existing buffer (no reallocation)
        if (!m_symTableBuffer.isValid())
        {
            cl_mem buf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                        bufSize, data.data(), &err);
            if (err != CL_SUCCESS)
                return MS::kFailure;
            m_symTableBuffer = MAutoCLMem(buf);
        }
        else
        {
            // Blocking write – ensures the local 'data' vector stays valid
            // until the transfer completes.  Because this path only runs when
            // the symmetry table is actually dirty (not every frame), the
            // blocking overhead is negligible.
            err = clEnqueueWriteBuffer(
                    MOpenCLInfo::getMayaDefaultOpenCLCommandQueue(),
                    m_symTableBuffer.get(),
                    CL_TRUE,            // blocking – host ptr must remain valid
                    0, bufSize, data.data(),
                    0, nullptr, nullptr);
            if (err != CL_SUCCESS)
                return MS::kFailure;
        }
    }

    return MS::kSuccess;
}

// ── uploadWeightsBuffer ──────────────────────────────────────────────────────
//
// Read per-vertex deformer weights from the node and upload to a GPU buffer.
// The same buffer-reuse strategy as uploadSymTableBuffer is used.
//
// Weights are stored in the deformer's weightList[geomIndex].weights multi-
// attribute.  Elements that have no key default to 1.0 (fully weighted).

MStatus TopoSymGPUDeformer::uploadWeightsBuffer(const MPlug&  outputPlug,
                                                 unsigned int  geomIndex,
                                                 unsigned int  numVerts)
{
    // Default all weights to 1.0
    std::vector<cl_float> weights(numVerts, 1.0f);

    MFnDependencyNode fnNode(outputPlug.node());
    MPlug weightListPlug = fnNode.findPlug(MPxDeformerNode::weightList, false);
    if (!weightListPlug.isNull())
    {
        MPlug geomWeightsPlug = weightListPlug.elementByLogicalIndex(geomIndex);
        if (!geomWeightsPlug.isNull())
        {
            MPlug weightsPlug = geomWeightsPlug.child(MPxDeformerNode::weights);
            if (!weightsPlug.isNull())
            {
                const unsigned int numElements = weightsPlug.numElements();
                for (unsigned int i = 0; i < numElements; ++i)
                {
                    MPlug wp           = weightsPlug.elementByPhysicalIndex(i);
                    const unsigned int li = wp.logicalIndex();
                    if (li < numVerts)
                        weights[li] = wp.asFloat();
                }
            }
        }
    }

    cl_int     err = CL_SUCCESS;
    cl_context ctx = MOpenCLInfo::getMayaDefaultOpenCLContextId();
    const size_t bufSize = numVerts * sizeof(cl_float);

    if (!m_weightsBuffer.isValid())
    {
        cl_mem buf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                    bufSize, weights.data(), &err);
        if (err != CL_SUCCESS)
            return MS::kFailure;
        m_weightsBuffer = MAutoCLMem(buf);
    }
    else
    {
        // Blocking write – ensures the local 'weights' vector stays valid
        // until the transfer completes.  Only runs when weights are dirty.
        err = clEnqueueWriteBuffer(
                MOpenCLInfo::getMayaDefaultOpenCLCommandQueue(),
                m_weightsBuffer.get(),
                CL_TRUE,            // blocking – host ptr must remain valid
                0, bufSize, weights.data(),
                0, nullptr, nullptr);
        if (err != CL_SUCCESS)
            return MS::kFailure;
    }

    return MS::kSuccess;
}

// ── enqueuePassthrough ───────────────────────────────────────────────────────
//
// When envelope == 0 the deformer is a no-op.  Rather than returning
// kDeformerRetryMainThread (which would stall the GPU pipeline), we issue a
// GPU-side buffer copy so that the output buffer is still properly populated
// and downstream GPU deformers can proceed without a CPU round-trip.

MPxGPUDeformer::DeformerStatus
TopoSymGPUDeformer::enqueuePassthrough(const MAutoCLMem&       inputBuffer,
                                        MAutoCLMem              outputBuffer,
                                        unsigned int            numVerts,
                                        const MAutoCLEventList& waitList,
                                        MAutoCLEvent&           outEvent)
{
    // Collect upstream events
    std::vector<cl_event> waitEvents;
    waitEvents.reserve(waitList.size());
    for (unsigned int i = 0; i < waitList.size(); ++i)
        if (waitList[i].isValid())
            waitEvents.push_back(waitList[i].get());

    cl_event copyEvent = nullptr;
    cl_int   err = clEnqueueCopyBuffer(
        MOpenCLInfo::getMayaDefaultOpenCLCommandQueue(),
        inputBuffer.get(), outputBuffer.get(),
        0, 0,
        static_cast<size_t>(numVerts) * 3 * sizeof(float),
        static_cast<cl_uint>(waitEvents.size()),
        waitEvents.empty() ? nullptr : waitEvents.data(),
        &copyEvent);

    if (err != CL_SUCCESS)
        return kDeformerRetryMainThread;

    outEvent = MAutoCLEvent(copyEvent);
    return kDeformerSuccess;
}

// ── runKernel ────────────────────────────────────────────────────────────────
//
// Set kernel arguments and enqueue the NDRange kernel.
// Global work size is padded to a multiple of localWorkSize so that every
// work-group is full (avoids tail-effect branch in the kernel on most GPUs).

MPxGPUDeformer::DeformerStatus
TopoSymGPUDeformer::runKernel(const MAutoCLMem&       inputBuffer,
                               MAutoCLMem              outputBuffer,
                               unsigned int            numVerts,
                               float                   envelope,
                               int                     mirrorAxis,
                               const MAutoCLEventList& waitList,
                               MAutoCLEvent&           outEvent)
{
    // ── Kernel arguments ────────────────────────────────────────────────────
    cl_kernel k = m_kernel.get();

    cl_mem inputMem  = inputBuffer.get();
    cl_mem outputMem = outputBuffer.get();
    cl_mem symMem    = m_symTableBuffer.get();
    cl_mem wgtMem    = m_weightsBuffer.get();
    cl_int nv        = static_cast<cl_int>(numVerts);
    cl_int axis      = static_cast<cl_int>(mirrorAxis);

    clSetKernelArg(k, 0, sizeof(cl_mem),   &inputMem);
    clSetKernelArg(k, 1, sizeof(cl_mem),   &outputMem);
    clSetKernelArg(k, 2, sizeof(cl_mem),   &symMem);
    clSetKernelArg(k, 3, sizeof(cl_mem),   &wgtMem);
    clSetKernelArg(k, 4, sizeof(cl_float), &envelope);
    clSetKernelArg(k, 5, sizeof(cl_int),   &nv);
    clSetKernelArg(k, 6, sizeof(cl_int),   &axis);

    // ── Work sizes ──────────────────────────────────────────────────────────
    // local work size of 64 is a safe default that performs well on both
    // NVIDIA and AMD GPU architectures.
    static const size_t kLocalWorkSize = 64;
    const size_t globalWorkSize =
        ((static_cast<size_t>(numVerts) + kLocalWorkSize - 1) / kLocalWorkSize)
        * kLocalWorkSize;

    // ── Upstream event wait-list ────────────────────────────────────────────
    std::vector<cl_event> waitEvents;
    waitEvents.reserve(waitList.size());
    for (unsigned int i = 0; i < waitList.size(); ++i)
        if (waitList[i].isValid())
            waitEvents.push_back(waitList[i].get());

    // ── Dispatch ────────────────────────────────────────────────────────────
    cl_event kernelEvent = nullptr;
    const cl_int err = clEnqueueNDRangeKernel(
        MOpenCLInfo::getMayaDefaultOpenCLCommandQueue(),
        k,
        1,                                                  // work dimensions
        nullptr,                                            // global_work_offset
        &globalWorkSize,
        &kLocalWorkSize,
        static_cast<cl_uint>(waitEvents.size()),
        waitEvents.empty() ? nullptr : waitEvents.data(),
        &kernelEvent);

    if (err != CL_SUCCESS)
    {
        MGlobal::displayError(
            MString("TopoSymGPUDeformer: clEnqueueNDRangeKernel failed, err=")
            + static_cast<int>(err));
        return kDeformerRetryMainThread;
    }

    outEvent = MAutoCLEvent(kernelEvent);
    return kDeformerSuccess;
}

// ── evaluate ─────────────────────────────────────────────────────────────────
//
// Main entry point called by Maya's parallel evaluation graph every frame.
//
// Frame N (typical – no attribute change):
//   1. Kernel already compiled            → skip initKernel()
//   2. symTable not dirty                 → skip GPU upload
//   3. weights not dirty                  → skip GPU upload
//   4. enqueue kernel                     → ~zero CPU work, all on GPU
//
// Frame 0 (first call) or after attribute change:
//   1. compile kernel (once)
//   2. upload symTable to GPU (once per topology change)
//   3. upload weights to GPU (once per weight change)
//   4. enqueue kernel
//
// The key insight: on the vast majority of animation frames only step 4
// executes, eliminating the CPU→GPU memory transfers that were the primary
// bottleneck in a naïve implementation.

MPxGPUDeformer::DeformerStatus
TopoSymGPUDeformer::evaluate(MDataBlock&             block,
                              const MEvaluationNode&  evaluationNode,
                              const MPlug&            outputPlug,
                              unsigned int            geomIndex,
                              const MAutoCLMem        inputBuffer,
                              unsigned int            inputComponents,
                              MAutoCLMem              outputBuffer,
                              unsigned int            outputComponents,
                              MAutoCLEvent&           waitForCompletion,
                              const MAutoCLEventList& geometryTaskEvents)
{
    MStatus status;

    // ── 1. Compile kernel (first call only) ──────────────────────────────────
    if (!m_kernel.isValid())
    {
        if (initKernel() != MS::kSuccess)
            return kDeformerRetryMainThread;
    }

    const unsigned int numVerts = inputComponents;
    if (numVerts == 0)
        return kDeformerSuccess;

    // ── 2. Envelope – read from data block (cheap, always current) ───────────
    const float env = block.inputValue(MPxDeformerNode::envelope, &status).asFloat();
    if (status != MS::kSuccess)
        return kDeformerRetryMainThread;

    // Short-circuit: no deformation needed, just pass the buffer through
    if (env <= 0.0f)
        return enqueuePassthrough(inputBuffer, outputBuffer, numVerts,
                                  geometryTaskEvents, waitForCompletion);

    // ── 3. Mirror axis ────────────────────────────────────────────────────────
    const int mirrorAxis =
        static_cast<int>(block.inputValue(TopoSymDeformer::aMirrorAxis, &status).asShort());
    if (status != MS::kSuccess)
        return kDeformerRetryMainThread;

    // ── 4. Symmetry table – upload to GPU only when dirty ────────────────────
    // Dirty on first frame, after topology change, or after symTable attribute
    // is modified.
    const bool symTablePlugDirty =
        evaluationNode.dirtyPlugExists(TopoSymDeformer::aSymTable, &status);

    if (m_symTableDirty || symTablePlugDirty)
    {
        MObject symData = block.inputValue(TopoSymDeformer::aSymTable, &status).data();
        if (status != MS::kSuccess)
            return kDeformerRetryMainThread;

        MFnIntArrayData fnIntArr(symData, &status);
        if (status != MS::kSuccess)
            return kDeformerRetryMainThread;

        if (uploadSymTableBuffer(fnIntArr.array()) != MS::kSuccess)
            return kDeformerRetryMainThread;

        m_symTableDirty = false;
        // If vertex count changed, weights buffer was also released inside
        // uploadSymTableBuffer – mark it dirty so it gets re-uploaded below.
        m_weightsDirty = true;
    }

    if (!m_symTableBuffer.isValid())
        return kDeformerRetryMainThread;

    // ── 5. Per-vertex weights – upload to GPU only when dirty ─────────────────
    // Dirty on first frame, after weight painting, or after topology change.
    const bool weightsPlugDirty =
        evaluationNode.dirtyPlugExists(MPxDeformerNode::weightList, &status);

    if (m_weightsDirty || weightsPlugDirty)
    {
        if (uploadWeightsBuffer(outputPlug, geomIndex, numVerts) != MS::kSuccess)
            return kDeformerRetryMainThread;
        m_weightsDirty = false;
    }

    if (!m_weightsBuffer.isValid())
        return kDeformerRetryMainThread;

    // ── 6. Dispatch kernel – the only work done on every frame ───────────────
    return runKernel(inputBuffer, outputBuffer, numVerts, env, mirrorAxis,
                     geometryTaskEvents, waitForCompletion);
}

// ── TopoSymGPUDeformerInfo ────────────────────────────────────────────────────

bool TopoSymGPUDeformerInfo::hasDeformerDependency(
    const MEvaluationNode& evaluationNode,
    const MPlug&           plug)
{
    MStatus status;
    // The GPU deformer depends on symTable, mirrorAxis, envelope, and weights.
    // Returning true here tells Maya that the GPU deformer must re-evaluate
    // when any of these plugs are dirty.
    return evaluationNode.dirtyPlugExists(TopoSymDeformer::aSymTable,      &status)
        || evaluationNode.dirtyPlugExists(TopoSymDeformer::aMirrorAxis,    &status)
        || evaluationNode.dirtyPlugExists(MPxDeformerNode::envelope,       &status)
        || evaluationNode.dirtyPlugExists(MPxDeformerNode::weightList,     &status);
}
