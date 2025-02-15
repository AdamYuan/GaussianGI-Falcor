import GaussianGI.Algorithm.ThreadGroupTilingX;

uniform uint2 gResolution;
RWTexture2D<float4> gIndirect;

#define GROUP_DIM 8

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint2 threadID = threadGroupTilingX((gResolution + GROUP_DIM - 1) / GROUP_DIM, uint2(GROUP_DIM), 8, groupThreadID.xy, groupID.xy);

    if (any(threadID >= gResolution))
        return;

    float4 indirect = gIndirect[threadID];
    indirect.xyz /= max(indirect.w, 1.0);
    indirect.w = 0;
    gIndirect[threadID] = indirect;
}
