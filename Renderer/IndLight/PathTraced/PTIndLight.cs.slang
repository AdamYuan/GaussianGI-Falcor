#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Util.ShaderUtil;
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
        float pdf;

        Ray ray;
        ray.origin = hitPoint.getRayOrigin();
        ray.tMin = 0;
        ray.tMax = FLT_MAX;
        ray.dir = alignDirection(hitPoint.normal, sample_cosine_hemisphere_concentric(sampleNext2D(sampleGen), pdf));
        if (dot(ray.dir, hitPoint.geomNormal) < 0)
            break;

        // float n_dot_wi = max(0, dot(hitPoint.normal, ray.dir));

        hitInfo = gGStaticScene.traceClosestHit(ray, true);
        if (!hitInfo.isHit)
        {
            // indLight += accumulate * M_1_PI * n_dot_wi * gGStaticScene.lighting.skyRadiance / pdf;
            indLight += accumulate * gGStaticScene.lighting.skyRadiance;
            break;
        }

        hitPoint = gGStaticScene.getHitPoint(hitInfo);
        float3 albedo = gGStaticScene.textures[hitPoint.textureID].SampleLevel(gGStaticScene.sampler, hitPoint.texcoord, 0).rgb;

        /* accumulate *= albedo * M_1_PI * n_dot_wi / pdf;
        indLight +=
            accumulate * M_1_PI * gGStaticScene.lighting.evaluateSunIrradiance(hitPoint.normal, gGShadow.evaluate(gGStaticScene, hitPoint));
        */

        accumulate *= albedo;
        indLight +=
            accumulate * M_1_PI * gGStaticScene.lighting.evaluateSunIrradiance(hitPoint.normal, gGShadow.evaluate(gGStaticScene, hitPoint));

        if (all(accumulate == 0))
            break;
    }

    if (any(isnan(indLight)) || any(isinf(indLight)))
        indLight = float3(0, 0, 0);

    float3 prevIndLight = gIndLight[threadID].rgb;
    indLight = (prevIndLight * gSPP + indLight) / float(gSPP + 1);
    gIndLight[threadID] = float4(indLight, 0);
}
