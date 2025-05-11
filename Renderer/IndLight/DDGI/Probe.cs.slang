#include "Utils/Math/MathConstants.slangh"
import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.SphericalHarmonics;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Renderer.Shadow.GShadow;
import Utils.Sampling.TinyUniformSampleGenerator;
import Utils.Sampling.UniformSampleGenerator;
import Utils.Math.MathHelpers;
import Grid;

#define SH_LEVEL 2
#if SH_LEVEL != 2
#error
#endif
#define SH_SIZE 9
#if SH_SIZE != 9
#error
#endif

#ifndef USE_FOLIAGE
#define USE_FOLIAGE 1
#endif

StructuredBuffer<SH9<float16_t3>> gPrevProbes;
RWStructuredBuffer<SH9<float16_t3>> gProbes;

uniform uint gTick;
uniform DDGI::Grid gGrid;
uniform GShadow gGShadow;

#define RAYS_PER_PROBE 32

#if RAYS_PER_PROBE != 32
#error
#endif

groupshared float gSharedSH[SH_SIZE * 3];

[numthreads(RAYS_PER_PROBE, 1, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint3 probeGridPos = groupID;
    uint probeGridIdx = gGrid.gridPos2GridIndex(probeGridPos);
    uint rayIdx = groupThreadID.x;

    if (rayIdx < SH_SIZE * 3)
        gSharedSH[rayIdx] = 0.0;

    GroupMemoryBarrierWithGroupSync();

    var randGen = UniformSampleGenerator(uint2(probeGridIdx, rayIdx), gTick);

    Ray ray;
    ray.origin = gGrid.gridPos2WorldPos(probeGridPos);
    ray.dir = sample_sphere(sampleNext2D(randGen)); // PDF = 1 / 4PI
    ray.tMin = 0;
    ray.tMax = FLT_MAX;

    float3 radiance = float3(0);

    var hitInfo = gGStaticScene.traceClosestHit(ray, bool(USE_FOLIAGE));
    if (!hitInfo.isHit)
    {
        radiance = gGStaticScene.lighting.skyRadiance;
    }
    else
    {
        var hitPoint = gGStaticScene.getHitPoint(hitInfo);
        float3 albedo = gGStaticScene.textures[hitPoint.textureID].SampleLevel(gGStaticScene.sampler, hitPoint.texcoord, 0).rgb;
        float3 brdf = M_1_PI * albedo;

        radiance += brdf * gGStaticScene.lighting.evaluateSunIrradiance(hitPoint.normal, gGShadow.evaluate(gGStaticScene, hitPoint));
        radiance += brdf * gGrid.convolveProbe(gPrevProbes, hitPoint.position, hitPoint.normal);
    }

    SH9<float> dirSh = shBasis<SH_LEVEL>(ray.dir);

    SH9<float3> sh = (SH<float3, SH_LEVEL>)0;
    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE; ++i)
        sh.data[i] = dirSh.data[i] * radiance * (M_4PI / float(RAYS_PER_PROBE));

    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE; ++i)
    {
        __atomic_add(gSharedSH[i * 3 + 0], sh.data[i][0]);
        __atomic_add(gSharedSH[i * 3 + 1], sh.data[i][1]);
        __atomic_add(gSharedSH[i * 3 + 2], sh.data[i][2]);
    }

    GroupMemoryBarrierWithGroupSync();

    // Wave lanes are not synchronized after rayQuery, WTF? Have to use shared memory for accumulation

    /* sh.data[1] = float3(WaveActiveSum(1.0) / float(RAYS_PER_PROBE), 0, 0);
    sh.data[2] = float3(WaveGetLaneCount() / float(RAYS_PER_PROBE), 0, 0); */

    if (rayIdx == 0)
    {
        [ForceUnroll]
        for (uint i = 0; i < SH_SIZE; ++i)
        {
            sh.data[i][0] = gSharedSH[i * 3 + 0];
            sh.data[i][1] = gSharedSH[i * 3 + 1];
            sh.data[i][2] = gSharedSH[i * 3 + 2];
        }

        if (gTick == 0)
        // if (true)
        {}
        else
        {
            SH<float3, SH_LEVEL> prevSh = shToFloat32(gPrevProbes[probeGridIdx]);
            [ForceUnroll]
            for (uint i = 0; i < SH_SIZE; ++i)
                sh.data[i] = lerp(sh.data[i], (float3)prevSh.data[i], 0.98);
        }

        gProbes[probeGridIdx] = shToFloat16(sh);
    }
}
