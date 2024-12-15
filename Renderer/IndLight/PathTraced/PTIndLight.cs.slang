#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Common.ShaderUtil;
import GaussianGI.Scene.GStaticScene;
import GaussianGI.Renderer.GVBuffer;
import GaussianGI.Renderer.Shadow.GShadow;
import Utils.Sampling.TinyUniformSampleGenerator;
import Utils.Math.MathHelpers;

#ifndef MAX_BOUNCE
#define MAX_BOUNCE 5
#endif

uniform uint gSPP;
uniform GVBuffer gGVBuffer;
uniform GShadow gGShadow;
RWTexture2D<float4> gIndLight;

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
        gIndLight[threadID] = float4(0, 0, 0, 0);
        return;
    }

    var sampleGen = TinyUniformSampleGenerator(threadID, gSPP);

    var hitPoint = gGStaticScene.getHitPoint(hitInfo);

    float3 accumulate = float3(1, 1, 1);
    float3 indLight = float3(0, 0, 0);
    for (uint bounce = 0; bounce < MAX_BOUNCE; ++bounce)
    {
        Ray ray;
        ray.origin = hitPoint.getRayOrigin();
        ray.tMin = 0;
        ray.tMax = FLT_MAX;
        ray.dir = alignDirection(hitPoint.geomNormal, sample_cosine_hemisphere_polar(sampleNext2D(sampleGen)));

        hitInfo = gGStaticScene.traceClosestHit(ray, true);
        if (!hitInfo.isHit)
            break;

        hitPoint = gGStaticScene.getHitPoint(hitInfo);
        float3 albedo = gGStaticScene.textures[hitPoint.textureID].SampleLevel(gGStaticScene.sampler, hitPoint.texcoord, 0).rgb;

        // outgoing radiance from light = (albedo / M_PI) * (n \dot w_l) * L_l * Vis
        accumulate *= albedo; // (albedo / M_PI) * L_i * (n \dot w_i) / ((n \dot w_i) / M_PI)
        indLight +=
            accumulate * M_1_PI * gGStaticScene.lighting.evaluateIrradiance(hitPoint.normal, gGShadow.evaluate(gGStaticScene, hitPoint));
    }

    float3 prevIndLight = gIndLight[threadID].rgb;
    indLight = (prevIndLight * gSPP + indLight) / float(gSPP + 1);
    gIndLight[threadID] = float4(indLight, 0);
}
