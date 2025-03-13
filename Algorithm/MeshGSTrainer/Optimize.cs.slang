#include "MeshGSTrainer.slangh"
import MeshGSTrainer;

using namespace MeshGSTrain;

#ifndef BATCH_SIZE
#define BATCH_SIZE 2
#endif

uniform uint gSplatCount;
RWSplatBuffer gSplats;
RWSplatBuffer gDLossDSplats;

RWSplatAdamBuffer gSplatAdams;
uniform Splat gLearnRate;
uniform float gAdamBeta1T, gAdamBeta2T;

#define ADAM_EPSILON 1e-8

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatID = threadID.x;
    if (splatID >= gSplatCount)
        return;

    var splatAdam = gSplatAdams.load<SplatAdam>(splatID);
    var splat_flat = gSplats.load<FlatSplat>(splatID);
    var dL_dsplat_flat = gDLossDSplats.load<FlatSplat>(splatID).mul(1.0 / float(BATCH_SIZE));
    gDLossDSplats.store(splatID, (Splat)0); // Clear splat gradients

    splatAdam.m = splatAdam.m.mul(ADAM_BETA1).add(dL_dsplat_flat.mul(1.0 - ADAM_BETA1));
    splatAdam.v = splatAdam.v.mul(ADAM_BETA2).add(dL_dsplat_flat.mul(dL_dsplat_flat).mul(1.0 - ADAM_BETA2));

    var hat_m = splatAdam.m.div(1.0 - gAdamBeta1T);
    var hat_v = splatAdam.v.div(1.0 - gAdamBeta2T);

    splat_flat = splat_flat.sub(hat_m.mul(gLearnRate.toSOA()).div(hat_v.sqrt().add(ADAM_EPSILON)));

    // if (!splat_flat.anyIsNanOrInf())
    {
        var splat = Splat::fromSOA(splat_flat);
        if (all(splat.geom.rotate == 0))
            splat.geom.rotate = float4(0, 0, 0, 1);
        splat.geom.rotate = normalize(splat.geom.rotate);
        splat.attrib.regularize();
        gSplats.store(splatID, splat);
    }
}

