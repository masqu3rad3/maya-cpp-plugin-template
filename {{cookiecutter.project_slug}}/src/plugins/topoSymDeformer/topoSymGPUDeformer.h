#pragma once

#include <maya/MPxGPUDeformer.h>
#include <maya/MGPUDeformerRegistrationInfo.h>
#include <maya/MAutoCLMem.h>
#include <maya/MAutoCLKernel.h>
#include <maya/MOpenCLInfo.h>
#include <maya/MEvaluationNode.h>
#include <maya/MPlug.h>
#include <maya/MDataBlock.h>
#include <maya/MIntArray.h>

#include <vector>

/**
 * TopoSymGPUDeformer – high-performance OpenCL path for TopoSymDeformer.
 *
 * Key design decisions for maximum performance
 * ─────────────────────────────────────────────
 * 1. Persistent GPU buffers
 *    The symmetry-table (int per vertex) and per-vertex weights (float per
 *    vertex) are uploaded to the GPU exactly ONCE per topology/weight change.
 *    Across animation frames they are reused without any CPU→GPU transfer.
 *
 * 2. Dirty-flag driven updates
 *    MEvaluationNode::dirtyPlugExists() is checked to detect actual attribute
 *    changes.  On most frames the symmetry table and weights are unchanged, so
 *    evaluate() only dispatches the OpenCL kernel – zero host transfers.
 *
 * 3. Single kernel compilation
 *    The OpenCL kernel is compiled from a source string literal the first time
 *    evaluate() is called, then stored in m_kernel.  Subsequent calls skip
 *    compilation entirely.
 *
 * 4. No CPU readback
 *    The function never copies GPU buffers back to CPU.  Input/output positions
 *    stay on the GPU throughout the entire deformer chain.
 *
 * 5. Proper GPU-pipeline event wiring
 *    geometryTaskEvents are forwarded to clEnqueueNDRangeKernel's wait-list so
 *    that the kernel starts as soon as upstream GPU work finishes – maximising
 *    GPU occupancy.
 *
 * 6. Envelope short-circuit
 *    When envelope == 0 the deformer is a no-op.  We perform an async buffer
 *    copy (still on GPU) instead of dispatching the kernel, avoiding any
 *    overhead.
 */
class TopoSymGPUDeformer : public MPxGPUDeformer
{
public:
    static MPxGPUDeformer*              creator();
    static MGPUDeformerRegistrationInfo* getGPUDeformerInfo();

    TopoSymGPUDeformer();
    ~TopoSymGPUDeformer() override;

    // ── MPxGPUDeformer interface ────────────────────────────────────────────
    DeformerStatus evaluate(MDataBlock&              block,
                            const MEvaluationNode&   evaluationNode,
                            const MPlug&             outputPlug,
                            unsigned int             geomIndex,
                            const MAutoCLMem         inputBuffer,
                            unsigned int             inputComponents,
                            MAutoCLMem               outputBuffer,
                            unsigned int             outputComponents,
                            MAutoCLEvent&            waitForCompletion,
                            const MAutoCLEventList&  geometryTaskEvents) override;

    void terminate() override;

private:
    // ── Helper methods ──────────────────────────────────────────────────────
    MStatus initKernel();

    MStatus uploadSymTableBuffer(const MIntArray& symTable);

    MStatus uploadWeightsBuffer(const MPlug&  outputPlug,
                                unsigned int  geomIndex,
                                unsigned int  numVerts);

    DeformerStatus enqueuePassthrough(const MAutoCLMem&       inputBuffer,
                                      MAutoCLMem              outputBuffer,
                                      unsigned int            numVerts,
                                      const MAutoCLEventList& waitList,
                                      MAutoCLEvent&           outEvent);

    DeformerStatus runKernel(const MAutoCLMem&       inputBuffer,
                             MAutoCLMem              outputBuffer,
                             unsigned int            numVerts,
                             float                   envelope,
                             int                     mirrorAxis,
                             const MAutoCLEventList& waitList,
                             MAutoCLEvent&           outEvent);

    // ── Persistent GPU resources (survive across frames) ───────────────────
    MAutoCLMem    m_symTableBuffer;   // cl_int  per vertex  (read-only)
    MAutoCLMem    m_weightsBuffer;    // cl_float per vertex  (read-only)
    MAutoCLKernel m_kernel;           // compiled OpenCL kernel

    // ── State ───────────────────────────────────────────────────────────────
    unsigned int  m_cachedNumVerts = 0;
    bool          m_symTableDirty  = true;
    bool          m_weightsDirty   = true;

    // ── Embedded kernel source (see topoSymGPUDeformer.cpp) ─────────────────
    static const char* s_kernelSource;
};

// ── GPU deformer registration info ──────────────────────────────────────────
class TopoSymGPUDeformerInfo : public MGPUDeformerRegistrationInfo
{
public:
    MPxGPUDeformer* createGPUDeformer() override
    {
        return TopoSymGPUDeformer::creator();
    }

    bool hasDeformerDependency(const MEvaluationNode& evaluationNode,
                               const MPlug&           plug) override;
};
