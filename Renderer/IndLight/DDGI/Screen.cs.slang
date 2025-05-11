#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Algorithm.ThreadGroupTilingX;
import GaussianGI.Renderer.GVBuffer;
import GaussianGI.Algorithm.SphericalHarmonics;
import GaussianGI.Scene.GStaticScene;
import Grid;

uniform uint2 gResolution;
RWTexture2D<float4> gIndirect;
uniform GVBuffer gGVBuffer;

StructuredBuffer<SH9<float16_t3>> gProbes;
uniform DDGI::Grid gGrid;

#define GROUP_DIM 8

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint2 threadID = threadGroupTilingX((gResolution + GROUP_DIM - 1) / GROUP_DIM, uint2(GROUP_DIM), 8, groupThreadID.xy, groupID.xy);

    if (any(threadID >= gResolution))
        return;

    var hitInfo = gGVBuffer.LoadHitInfo(threadID);
    if (!hitInfo.isHit)
        return;
    var hitPoint = gGStaticScene.getHitPoint(hitInfo);
    float3 normal = hitPoint.normal;
    float3 indirect = M_1_PI * gGrid.convolveProbe(gProbes, hitPoint.position, hitPoint.normal);
    gIndirect[threadID] = float4(indirect, 0);
}
