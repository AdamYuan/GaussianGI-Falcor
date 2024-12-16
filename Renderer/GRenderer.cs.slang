#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Util.ShaderUtil;
import GaussianGI.Scene.GStaticScene;
import GVBuffer;
import Shadow.GShadow;

uniform GVBuffer gGVBuffer;
uniform GShadow gGShadow;
RWTexture2D<unorm float4> gTarget;
Texture2D<float4> gIndLight;

#ifndef TARGET_VAR_NAME
#define TARGET_VAR_NAME radiance
#endif

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
        gTarget[threadID] = float4(gGStaticScene.lighting.skyRadiance, 1);
        return;
    }

    var hitPoint = gGStaticScene.getHitPoint(hitInfo);

    float3 albedo = gGVBuffer.LoadAlbedo(threadID);
    float3 clampedNormal = hitPoint.normal * 0.5 + 0.5;
    float shadow = gGShadow.evaluate(gGStaticScene, hitPoint);
    float3 direct = M_1_PI * gGStaticScene.lighting.evaluateSunIrradiance(hitPoint.normal, shadow);
    float3 indirect = gIndLight.Load(int3(threadID, 0)).rgb;
    float3 directRadiance = albedo * direct;
    float3 radiance = albedo * (direct + indirect);

    gTarget[threadID] = float4(float3(TARGET_VAR_NAME), 1);
    // gTarget[threadID] = float4(hitPoint.normal * 0.5 + 0.5, 1);
}
