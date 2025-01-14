import MeshGSTrainer;

using namespace MeshGSTrainer;

#ifndef BATCH_SIZE
#define BATCH_SIZE 2
#endif

uniform uint gSplatCount;
RWSplatBuffer gSplats;
RWSplatBuffer gDLossDSplats;

RWSplatAdamBuffer gSplatAdams;
uniform float gAdamBeta1, gAdamBeta2;
uniform float gAdamBeta1T, gAdamBeta2T;
uniform float gAdamLearnRate, gAdamEpsilon;

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

    splatAdam.m = splatAdam.m.mul(gAdamBeta1).add(dL_dsplat_flat.mul(1.0 - gAdamBeta1));
    splatAdam.v = splatAdam.v.mul(gAdamBeta2).add(dL_dsplat_flat.mul(dL_dsplat_flat).mul(1.0 - gAdamBeta2));

    FlatSplat hat_m = splatAdam.m.div(1.0 - gAdamBeta1T);
    FlatSplat hat_v = splatAdam.v.div(1.0 - gAdamBeta2T);

    splat_flat = splat_flat.sub(hat_m.mul(gAdamLearnRate).div(hat_v.sqrt().add(gAdamEpsilon)));

    var splat = Splat::fromSOA(splat_flat);
    splat.rotate = normalize(splat.rotate);
    gSplats.store(splatID, splat);
}

