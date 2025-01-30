#include "MeshGSTrainer.slangh"
import MeshGSTrainer;

using namespace MeshGSTrain;

StructuredBuffer<uint> gSplatViewDrawArgs; // drawArgs[1] is instanceCount also splatViewCount
Trait::RWSplatViewBuffer gDLossDSplatViews;
Trait::SplatBuffer gSplats;
Trait::RWSplatBuffer gDLossDSplats;
StructuredBuffer<uint> gSplatViewSplatIDs;

uniform Camera gCamera;
uniform float2 gResolution;

#ifndef BATCH_SIZE
#define BATCH_SIZE 2
#endif

[numthreads(BACKWARD_VIEW_DIM, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatViewCount = gSplatViewDrawArgs[1], splatViewID = threadID.x;
    if (splatViewID >= splatViewCount)
        return;
    var dL_dsplatView = gDLossDSplatViews.load<Trait::SplatView>(splatViewID);
    gDLossDSplatViews.store(splatViewID, (Trait::SplatView)0); // Clear splatView gradients
    uint splatID = gSplatViewSplatIDs[splatViewID];
    var splat = gSplats.load<Trait::Splat>(splatID);

    var dp = diffPair(splat);
    bwd_diff(splat2splatViewGSPP)(
        dp, //
        gCamera.viewMat,
        gCamera.projMat,
        gCamera.nearZ,
        gCamera.farZ,
        gResolution,
        dL_dsplatView
    );

    var dL_dsplat_flat = dp.d.toSOA();
    if (BATCH_SIZE > 1)
        dL_dsplat_flat = dL_dsplat_flat.add(gDLossDSplats.load<Trait::FlatSplat>(splatID));
    gDLossDSplats.store(splatID, dL_dsplat_flat);
}
