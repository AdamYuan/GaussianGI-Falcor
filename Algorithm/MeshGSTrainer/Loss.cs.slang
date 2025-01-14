import GaussianGI.Algorithm.ThreadGroupTilingX;
import MeshGSTrainer;

using namespace MeshGSTrainer;

#define GROUP_DIM 8

SplatRTTexture gSplatRT;
MeshRTTexture gMeshRT;
RWSplatTexture gDLossDCs_Ts;

uniform uint2 gResolution;

[Differentiable]
float getLossL1(SplatChannel predict, no_diff SplatChannel data)
{
    var d = predict.sub(data);
    return d.abs().sum();
}

[Differentiable]
float getLossL2(SplatChannel predict, no_diff SplatChannel data)
{
    var d = predict.sub(data);
    return d.mul(d).sum();
}

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint2 threadID = threadGroupTilingX((gResolution + GROUP_DIM - 1) / GROUP_DIM, uint2(GROUP_DIM), 8, groupThreadID.xy, groupID.xy);

    if (any(threadID >= gResolution))
        return;

    float T;
    SplatChannel predict = gSplatRT.load(threadID, T);
    SplatChannel data = gMeshRT.load(threadID);

    var dp = diffPair(predict);
    bwd_diff(getLossL2)(dp, data, 1.0);

    SplatChannel dL_dC = dp.d;
    gDLossDCs_Ts.store(threadID, dL_dC, T);
}
