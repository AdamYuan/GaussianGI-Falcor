#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Algorithm.ShaderMisc;
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
    var hitPoint = gGStaticScene.getHitPoint(hitInfo);

    float3 albedo = select(hitInfo.isHit, gGVBuffer.LoadAlbedo(threadID), 1);
    float3 clampedNormal = hitPoint.normal * 0.5 + 0.5;
    float shadow = select(hitInfo.isHit, gGShadow.evaluate(gGStaticScene, hitPoint), 0);
    float3 direct = select(
        hitInfo.isHit, M_1_PI * gGStaticScene.lighting.evaluateSunIrradiance(hitPoint.normal, shadow), gGStaticScene.lighting.skyRadiance
    );
    float3 indirect = select(hitInfo.isHit, gIndLight.Load(int3(threadID, 0)).rgb, 0);
    float3 directRadiance = albedo * direct;
    float3 radiance = albedo * (direct + indirect);
    float viewZ = select(hitInfo.isHit, mul(gGStaticScene.camera.data.viewMat, float4(hitPoint.position, 1.0)).z, 0);
    float viewZRemap = -viewZ / gGStaticScene.camera.data.farZ;

    gTarget[threadID] = float4(float3(TARGET_VAR_NAME), 1);
    // gTarget[threadID] = float4(hitPoint.normal * 0.5 + 0.5, 1);
}
