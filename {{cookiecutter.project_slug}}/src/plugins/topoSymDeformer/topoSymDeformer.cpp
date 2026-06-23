#include "topoSymDeformer.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MItGeometry.h>
#include <maya/MPointArray.h>
#include <maya/MMatrix.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MGlobal.h>

// ── Static member definitions ────────────────────────────────────────────────

const MTypeId TopoSymDeformer::kNodeId(0x00127890);
const MString TopoSymDeformer::kNodeName("topoSymDeformer");

MObject TopoSymDeformer::aSymTable;
MObject TopoSymDeformer::aMirrorAxis;

// ── Creator / Initialize ─────────────────────────────────────────────────────

void* TopoSymDeformer::creator()
{
    return new TopoSymDeformer();
}

MStatus TopoSymDeformer::initialize()
{
    MStatus status;

    MFnTypedAttribute tAttr;
    aSymTable = tAttr.create("symTable", "st", MFnData::kIntArray, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    tAttr.setStorable(true);
    tAttr.setKeyable(false);
    tAttr.setConnectable(true);
    status = addAttribute(aSymTable);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MFnEnumAttribute eAttr;
    aMirrorAxis = eAttr.create("mirrorAxis", "ma", 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    eAttr.addField("X", 0);
    eAttr.addField("Y", 1);
    eAttr.addField("Z", 2);
    eAttr.setStorable(true);
    eAttr.setKeyable(true);
    status = addAttribute(aMirrorAxis);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    // Recompute output when these inputs change
    status = attributeAffects(aSymTable,   outputGeom);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = attributeAffects(aMirrorAxis, outputGeom);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return MS::kSuccess;
}

// ── deform ───────────────────────────────────────────────────────────────────
//
// CPU fallback.  Called when the GPU deformer is unavailable (no GPU override,
// or when the GPU deformer returns kDeformerRetryMainThread).
//
// Performance notes
//   • iter.allPositions() / setAllPositions() batch-reads and batch-writes the
//     entire position array in one call, which is significantly faster than
//     calling iter.position() / setPosition() per vertex.
//   • The symmetry table is read from the data block once per evaluation.
//   • Only vertices that have a valid mirror and non-zero weight are modified.
//
MStatus TopoSymDeformer::deform(MDataBlock&    block,
                                 MItGeometry&   iter,
                                 const MMatrix& /*localToWorldMatrix*/,
                                 unsigned int   geomIndex)
{
    MStatus status;

    // Envelope (global blend)
    const float env = block.inputValue(envelope, &status).asFloat();
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (env <= 0.0f)
        return MS::kSuccess;

    // Mirror axis
    const int axis = static_cast<int>(
        block.inputValue(aMirrorAxis, &status).asShort());
    CHECK_MSTATUS_AND_RETURN_IT(status);

    // Symmetry table
    MObject symData = block.inputValue(aSymTable, &status).data();
    CHECK_MSTATUS_AND_RETURN_IT(status);
    MFnIntArrayData fnIntArr(symData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MIntArray symTable = fnIntArr.array();
    const unsigned int tableLen = symTable.length();
    if (tableLen == 0)
        return MS::kSuccess;

    // Batch-read all vertex positions in one call (much faster than per-vertex)
    MPointArray positions;
    status = iter.allPositions(positions, MSpace::kObject);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const unsigned int numVerts = positions.length();

    for (unsigned int i = 0; i < numVerts; ++i)
    {
        if (i >= tableLen)
            break;

        const int srcIdx = symTable[i];
        if (srcIdx < 0 || static_cast<unsigned int>(srcIdx) >= numVerts || srcIdx == static_cast<int>(i))
            continue;

        // Per-vertex weight blended with envelope
        const float w = weightValue(block, geomIndex, i) * env;
        if (w <= 0.0f)
            continue;

        MPoint src = positions[static_cast<unsigned int>(srcIdx)];

        // Mirror across the specified axis
        switch (axis)
        {
            case 0: src.x = -src.x; break;
            case 1: src.y = -src.y; break;
            case 2: src.z = -src.z; break;
            default: break;
        }

        // Blend
        const float inv = 1.0f - w;
        MPoint& dst = positions[i];
        dst.x = dst.x * inv + src.x * w;
        dst.y = dst.y * inv + src.y * w;
        dst.z = dst.z * inv + src.z * w;
    }

    // Batch-write all positions back in one call
    status = iter.setAllPositions(positions, MSpace::kObject);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return MS::kSuccess;
}
