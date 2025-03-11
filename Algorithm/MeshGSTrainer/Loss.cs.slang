import GaussianGI.Algorithm.ThreadGroupTilingX;
import MeshGSTrainer;

using namespace MeshGSTrain;

#define GROUP_DIM 8

Trait::SplatRTTexture gSplatRT;
Trait::MeshRTTexture gMeshRT;
RWSplatTexture gDLossDCs_Ts;

uniform uint2 gResolution;

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint2 threadID = threadGroupTilingX((gResolution + GROUP_DIM - 1) / GROUP_DIM, uint2(GROUP_DIM), 8, groupThreadID.xy, groupID.xy);

    if (any(threadID >= gResolution))
        return;

    float T;
    Trait::SplatChannel predict = gSplatRT.load(threadID, T);
    Trait::SplatChannel data = gMeshRT.load(threadID);

    uint pixelCount = gResolution.x * gResolution.y;

    var dp = diffPair(predict);
    bwd_diff(Trait::loss)(dp, data, pixelCount, 1.0);

    Trait::SplatChannel dL_dC = dp.d;
    gDLossDCs_Ts.store(threadID, SplatChannelT(dL_dC, T));
}
