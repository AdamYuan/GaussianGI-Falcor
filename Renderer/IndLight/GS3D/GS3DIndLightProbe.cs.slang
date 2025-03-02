#include "Utils/Math/MathConstants.slangh"
#include "GaussianGI/Algorithm/Icosahedron.slangh"
import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.SGGX;
import GaussianGI.Algorithm.SphericalHarmonics;
import GaussianGI.Renderer.Shadow.GShadow;
import GS3DIndLightSplat;
import Utils.Math.Quaternion;
import Utils.Sampling.TinyUniformSampleGenerator;
import Utils.Math.MathHelpers;

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;

RWBuffer<float> gSplatShadows;
StructuredBuffer<SH9<float3>> gPrevSplatProbes;
RWStructuredBuffer<SH9<float3>> gSplatProbes;

RaytracingAccelerationStructure gSplatAccel;

uniform uint gTick;

#define RAYS_PER_PROBE 32

#if RAYS_PER_PROBE != 32
#error
#endif

[numthreads(RAYS_PER_PROBE, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID, uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint splatIdx = groupID.x;
    uint splatRayIdx = groupThreadID.x;
    var splat = gSplats.load(splatIdx);

    var randGen = TinyUniformSampleGenerator(uint2(splatIdx, splatRayIdx), gTick);

    Ray ray;
    ray.tMin = 0.0;
    ray.tMax = FLT_MAX;
    ray.origin = splat.geom.mean;
    ray.dir = sample_sphere(sampleNext2D(randGen)); // PDF = 1 / 4PI

    float3 radiance;
    bool error = false;

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_CULL_BACK_FACING_TRIANGLES> rayQuery;

    rayQuery.TraceRayInline(gSplatAccel, RAY_FLAG_NONE, 0xFF, ray.toRayDesc());

    while (rayQuery.Proceed())
        ;

    if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        uint hitInstanceIdx = rayQuery.CommittedInstanceIndex();
        uint meshSplatOfst = rayQuery.CommittedPrimitiveIndex() / ICOSAHEDRON_TRIANGLE_COUNT;
        uint hitMeshSplatIdx = rayQuery.CommittedInstanceID() + meshSplatOfst;
        uint hitSplatIdx = gSplats.loadInstanceDesc(hitInstanceIdx).firstSplatIdx + meshSplatOfst;

        var hitSplat = gSplats.load(hitMeshSplatIdx, hitInstanceIdx);

        float3x3 hitSplatRotMatT = float3x3(
            rotate_vector(float3(1, 0, 0), hitSplat.geom.rotate),
            rotate_vector(float3(0, 1, 0), hitSplat.geom.rotate),
            rotate_vector(float3(0, 0, 1), hitSplat.geom.rotate)
        );
        float3 hitSplatInvScale = 1.0 / hitSplat.geom.scale;
        float3 o_g = mul(hitSplatRotMatT, ray.origin - hitSplat.geom.mean) * hitSplatInvScale;
        float3 d_g = mul(hitSplatRotMatT, ray.dir) * hitSplatInvScale;
        float t_max = -dot(o_g, d_g) / dot(d_g, d_g); // 3D Gaussian Ray Tracing, 4.3 Evaluating Particle Response
        float3 x = ray.eval(t_max);
        float3 x_g = mul(hitSplatRotMatT, x - hitSplat.geom.mean) * hitSplatInvScale;
        float power = max(dot(x_g, x_g), 0);
        float alpha = exp(-0.5 * power);
        if (isinf(alpha) || isnan(alpha))
            alpha = 0.0;
        alpha = saturate(alpha);

        float3 hitNormal = SGGX::getSplatNormal(transpose(hitSplatRotMatT), hitSplat.geom.scale, ray.dir);
        SH9<float3> hitProbe = gPrevSplatProbes[hitSplatIdx];

        float3 hitRadiance = max((float3)shDiffuse(hitProbe, shBasis<2>(hitNormal)), 0.0);
        hitRadiance += gGStaticScene.lighting.evaluateSunIrradiance(hitNormal, gSplatShadows[hitSplatIdx]);
        hitRadiance = M_1_PI * hitSplat.attrib.albedo * hitRadiance;
        // float3 hitRadiance = hitSplat.attrib.albedo;
        // hitRadiance = gGStaticScene.lighting.skyRadiance;

        float3 missRadiance = max((float3)shDot(hitProbe, shBasis<2>(ray.dir)), 0.0);
        // float3 missRadiance = gGStaticScene.lighting.skyRadiance;

        radiance = lerp(missRadiance, hitRadiance, alpha);

        if (any(isinf(radiance)) || any(isnan(radiance)))
            radiance = 0;

        if (splatIdx == hitSplatIdx)
            error = true;
    }
    else
        radiance = gGStaticScene.lighting.skyRadiance;

    SH9<float> dirSh = shBasis<2>(ray.dir);

    SH9<float3> sh;

    [ForceUnroll]
    for (uint i = 0; i < sh.data.getCount(); ++i)
        sh.data[i] = dirSh.data[i] * radiance;

    [ForceUnroll]
    for (uint i = 0; i < sh.data.getCount(); ++i)
        sh.data[i] = WaveActiveSum(sh.data[i]);

    [ForceUnroll]
    for (uint i = 0; i < sh.data.getCount(); ++i)
        sh.data[i] *= M_4PI / float(RAYS_PER_PROBE);

    if (splatRayIdx == 0)
    {
        // if (gTick == 0)
        if (true)
        {
        }
        else
        {
            SH9<float3> prevSh = gPrevSplatProbes[splatIdx];
            [ForceUnroll]
            for (uint i = 0; i < sh.data.getCount(); ++i)
                sh.data[i] = lerp(sh.data[i], (float3)prevSh.data[i], 0.98);
        }

        /* if (error)
        {
            [ForceUnroll]
            for (uint i = 0; i < sh.data.getCount(); ++i)
                sh.data[i] = float3(0, 1, 1);
        } */

        gSplatProbes[splatIdx] = sh;
    }
}
