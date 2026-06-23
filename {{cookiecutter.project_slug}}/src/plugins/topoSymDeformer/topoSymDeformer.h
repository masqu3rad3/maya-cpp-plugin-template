#pragma once

#include <maya/MPxDeformerNode.h>
#include <maya/MTypeId.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>

/**
 * TopoSymDeformer – CPU path of the topological-symmetry deformer.
 *
 * Attributes
 *   • symTable   (intArray)  – per-vertex symmetry mapping.
 *                              symTable[i] = j  →  vertex i should mirror vertex j.
 *                              symTable[i] = -1 →  no mirror (pass-through).
 *   • mirrorAxis (enum 0-2)  – axis to negate when mirroring (0=X, 1=Y, 2=Z).
 *   • envelope   (float)     – inherited from MPxDeformerNode; global blend [0,1].
 *   • weightList (painted)   – inherited per-vertex weights.
 *
 * The GPU path is handled by TopoSymGPUDeformer (topoSymGPUDeformer.h).
 */
class TopoSymDeformer : public MPxDeformerNode
{
public:
    static void*    creator();
    static MStatus  initialize();

    MStatus deform(MDataBlock& block,
                   MItGeometry& iter,
                   const MMatrix& localToWorldMatrix,
                   unsigned int geomIndex) override;

    // Node identity
    static const MTypeId kNodeId;
    static const MString kNodeName;

    // Custom attributes (also read by the GPU deformer)
    static MObject aSymTable;
    static MObject aMirrorAxis;
};
