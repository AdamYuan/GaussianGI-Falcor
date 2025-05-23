#include "MeshGSTrainer.slangh"
import MeshGSTrainer;

using namespace MeshGSTrain;

StructuredBuffer<uint> gSplatViewDrawArgs; // drawArgs[1] is instanceCount also splatViewCount
RWSplatViewBuffer gDLossDSplatViews;
SplatBuffer gSplats;
RWSplatBuffer gDLossDSplats;
StructuredBuffer<uint> gSplatViewSplatIDs;

RWStructuredBuffer<SplatAccumGrad> gSplatAccumGrads;

uniform Camera gCamera;

#ifndef BATCH_SIZE
#define BATCH_SIZE 2
#endif

[numthreads(BACKWARD_VIEW_DIM, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatViewCount = gSplatViewDrawArgs[1], splatViewID = threadID.x;
    if (splatViewID >= splatViewCount)
        return;
    var dL_dsplatView = gDLossDSplatViews.load<SplatView>(splatViewID);
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
        dL_dsplatView
    );

    var dL_dsplat_flat = dp.d.toSOA();
    if (dL_dsplat_flat.anyIsNanOrInf())
        dL_dsplat_flat = (FlatSplat)0;

    if (BATCH_SIZE > 1)
        dL_dsplat_flat = dL_dsplat_flat.add(gDLossDSplats.load<FlatSplat>(splatID));
    gDLossDSplats.store(splatID, dL_dsplat_flat);

    float posGrad = length(dp.d.geom.mean);
    if (!isnan(posGrad) && !isinf(posGrad))
    {
        SplatAccumGrad accumGrad = gSplatAccumGrads[splatID];
        accumGrad.append(posGrad);
        gSplatAccumGrads[splatID] = accumGrad;
    }
}
