#include "MeshGSTrainer.slangh"
import MeshGSTrainer;

using namespace MeshGSTrainer;

StructuredBuffer<uint> gSplatViewDrawArgs; // drawArgs[1] is instanceCount also splatViewCount
RWSplatViewBuffer gDLossDSplatViews;
SplatBuffer gSplats;
RWSplatBuffer gDLossDSplats;
StructuredBuffer<uint> gSplatViewSplatIDs;

uniform Camera gCamera;
uniform float2 gResolution;

[numthreads(BACKWARD_VIEW_DIM, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatViewCount = gSplatViewDrawArgs[1], splatViewID = threadID.x;
    if (splatViewID >= splatViewCount)
        return;
    SplatView dL_dsplatView = gDLossDSplatViews.load(splatViewID);
    gDLossDSplatViews.store(splatViewID, (SplatView)0); // Clear splatView gradients
    uint splatID = gSplatViewSplatIDs[splatViewID];
    var splat = gSplats.load<Splat>(splatID);

    var dp = diffPair(splat);
    bwd_diff(splat2splatView)(
        dp, //
        gCamera.viewMat,
        gCamera.projMat,
        gCamera.nearZ,
        gCamera.farZ,
        gResolution,
        dL_dsplatView
    );

    Splat dL_dsplat = dp.d;
    gDLossDSplats.store(splatID, dL_dsplat);
}
