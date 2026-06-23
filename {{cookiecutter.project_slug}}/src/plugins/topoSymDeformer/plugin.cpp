#include "topoSymDeformer.h"
#include "topoSymGPUDeformer.h"

#include <maya/MFnPlugin.h>
#include <maya/MGPUDeformerRegistry.h>
#include <maya/MPxDeformerNode.h>

// ─────────────────────────────────────────────────────────────────────────────
// initializePlugin
// ─────────────────────────────────────────────────────────────────────────────
MStatus initializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj, "masqu3rad3", "1.0", "Any");

    // Register the CPU deformer node
    status = plugin.registerNode(
        TopoSymDeformer::kNodeName,
        TopoSymDeformer::kNodeId,
        TopoSymDeformer::creator,
        TopoSymDeformer::initialize,
        MPxNode::kDeformerNode);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    // Register the GPU override for the deformer
    // The GPU deformer is automatically selected by Maya when GPU Override is
    // enabled and the deformer is in the evaluation graph.
    status = MGPUDeformerRegistry::registerGPUDeformerCreator(
        TopoSymDeformer::kNodeName,
        TopoSymDeformer::kNodeId,
        TopoSymGPUDeformer::getGPUDeformerInfo());
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return MS::kSuccess;
}

// ─────────────────────────────────────────────────────────────────────────────
// uninitializePlugin
// ─────────────────────────────────────────────────────────────────────────────
MStatus uninitializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj);

    // Deregister GPU override first
    status = MGPUDeformerRegistry::deregisterGPUDeformerCreator(
        TopoSymDeformer::kNodeName,
        TopoSymDeformer::kNodeId);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    // Deregister CPU node
    status = plugin.deregisterNode(TopoSymDeformer::kNodeId);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return MS::kSuccess;
}
