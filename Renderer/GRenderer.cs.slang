#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Common.ShaderUtil;
import GaussianGI.Scene.GStaticScene;
import GVBuffer;
import Shadow.GShadow;

GVBuffer gGVBuffer;
GShadow gGShadow;
RWTexture2D<unorm float4> gTarget;

#define GROUP_DIM 8

[numthreads(GROUP_DIM, GROUP_DIM, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint2 threadID =
        threadGroupTilingX((gGVBuffer.resolution + GROUP_DIM - 1) / GROUP_DIM, uint2(GROUP_DIM), 8, groupThreadID.xy, groupID.xy);

    if (any(threadID >= gGVBuffer.resolution))
        return;

    var hitInfo = gGVBuffer.LoadHitInfo(threadID);
    if (!hitInfo.isHit)
    {
        gTarget[threadID] = float4(0, 0, 0, 1);
        return;
    }

    var hitPoint = gGStaticScene.getHitPoint(hitInfo);

    float3 brdf = gGVBuffer.LoadAlbedo(threadID) * M_1_PI;
    float shadow = gGShadow.evaluate(gGStaticScene, hitPoint);
    float3 irradiance = gGStaticScene.lighting.evaluateIrradiance(hitPoint.normal, shadow);
    float3 radiance = brdf * irradiance;

    gTarget[threadID] = float4(radiance, 1);
    // gTarget[threadID] = float4(hitPoint.normal * 0.5 + 0.5, 1);
}
