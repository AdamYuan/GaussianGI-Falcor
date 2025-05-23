#include "Utils/Math/MathConstants.slangh"
#include "GaussianGI/Renderer/IndLight/GS3D/GS3DAccelStructPrimitive.slangh"
#include "GaussianGI/Algorithm/SphericalHarmonics.slangh"
#include "GaussianGI/Algorithm/GS3DBound.slangh"
import GaussianGI.Scene.GStaticScene;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.SGGX;
import GaussianGI.Algorithm.SphericalHarmonics;
import GaussianGI.Algorithm.ShaderMisc;
import GaussianGI.Renderer.Shadow.GShadow;
import GS3DIndLightSplat;
import Utils.Sampling.TinyUniformSampleGenerator;
import Utils.Sampling.UniformSampleGenerator;
import Utils.Math.MathHelpers;

#ifndef VNDF_SAMPLE
#define VNDF_SAMPLE 1
#endif

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;

#define SH_LEVEL 2

RWBuffer<float> gSplatShadows;
StructuredBuffer<SH<float16_t3, SH_LEVEL>> gPrevSplatProbes;
RWStructuredBuffer<SH<float16_t3, SH_LEVEL>> gSplatProbes;

RaytracingAccelerationStructure gSplatAccel;

uniform uint gTick;

#define RAYS_PER_PROBE 32

#if RAYS_PER_PROBE != 32
#error
#endif

groupshared float gSharedSH[SH_SIZE(SH_LEVEL) * 3];

float getSplatTMin(Splat splat, float3 dir)
{
    return 0;

    float3x3 splatRotMat = GS3D::getQuatMatrix(splat.geom.rotate);
    dir = mul(transpose(splatRotMat), dir);
    float3 det = dir / splat.geom.scale;
    det *= det;
    float t = (GS3D_BOUND_SQRT_2LOG_100) / sqrt(det.x + det.y + det.z);
    // float t = 1.0 / sqrt(det.x + det.y + det.z);
    if (isnan(t) || isinf(t))
        return 0;
    return t;
}

[numthreads(RAYS_PER_PROBE, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID, uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    uint splatIdx = groupID.x;
    uint splatRayIdx = groupThreadID.x;
    var splat = gSplats.load(splatIdx);

    if (splatRayIdx < SH_SIZE(SH_LEVEL) * 3)
        gSharedSH[splatRayIdx] = 0.0;

    GroupMemoryBarrierWithGroupSync();

    var randGen = UniformSampleGenerator(uint2(splatIdx, splatRayIdx), gTick);

    Ray ray;
    ray.dir = sample_sphere(sampleNext2D(randGen)); // PDF = 1 / 4PI
    ray.origin = splat.geom.mean;
    ray.tMin = getSplatTMin(splat, ray.dir);
    ray.tMax = FLT_MAX;

    float3 radiance;
    bool error = false;

    RayQuery<RAY_FLAG_FORCE_OPAQUE | AS_PRIM_CULL_FLAG> rayQuery;

    rayQuery.TraceRayInline(gSplatAccel, RAY_FLAG_NONE, 0xFF, ray.toRayDesc());

    while (rayQuery.Proceed())
        ;

    if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        uint hitInstanceIdx = rayQuery.CommittedInstanceIndex();
        uint meshSplatOfst = rayQuery.CommittedPrimitiveIndex() / AS_PRIM_TRIANGLE_COUNT;
        uint hitMeshSplatIdx = rayQuery.CommittedInstanceID() + meshSplatOfst;
        uint hitSplatIdx = gSplats.loadInstanceDesc(hitInstanceIdx).firstSplatIdx + meshSplatOfst;

        var hitSplat = gSplats.load(hitMeshSplatIdx, hitInstanceIdx);

        float3x3 hitSplatRotMat = GS3D::getQuatMatrix(hitSplat.geom.rotate);

        float3 w_i = -ray.dir;
        float3x3 w_i_rotMatT = getRobustAlignRotMatT(w_i);

        float2x3 hitSplatCov3D = GS3D::getCovariance3D(hitSplatRotMat, hitSplat.geom.scale);
        float hitAlpha = GS3D::getRayAlpha(ray.origin, w_i_rotMatT, hitSplatCov3D, hitSplat.geom.mean);

#if VNDF_SAMPLE
        float3 hitNormal = SGGX::VNDFSampler::fromSplat(hitSplatRotMat, hitSplat.geom.scale, w_i_rotMatT).sample(sampleNext2D(randGen));
#else
        float3 hitNormal = SGGX::getSplatNormal(hitSplatRotMat, hitSplat.geom.scale, w_i_rotMatT);
#endif
        SH<float3, SH_LEVEL> hitProbe = (SH<float3, SH_LEVEL>)0;
        if (gTick > 0)
            hitProbe = shToFloat32(gPrevSplatProbes[hitSplatIdx]);

        float3 hitRadiance = float3(0);
        hitRadiance += max((float3)shDiffuse(hitProbe, shBasis<SH_LEVEL>(hitNormal)), 0.0);
        hitRadiance += gGStaticScene.lighting.evaluateSunIrradiance(hitNormal, gSplatShadows[hitSplatIdx]);
        hitRadiance = M_1_PI * hitSplat.attrib.albedo * hitRadiance;

        float3 missRadiance = max((float3)shDot(hitProbe, shBasis<SH_LEVEL>(ray.dir)), 0.0);

        radiance = lerp(missRadiance, hitRadiance, hitAlpha);

        if (splatIdx == hitSplatIdx)
            error = true;
    }
    else
        radiance = gGStaticScene.lighting.skyRadiance;

    // radiance = gGStaticScene.lighting.skyRadiance;

    SH<float, SH_LEVEL> dirSh = shBasis<SH_LEVEL>(ray.dir);

    SH<float3, SH_LEVEL> sh = (SH<float3, SH_LEVEL>)0;

    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE(SH_LEVEL); ++i)
        sh.data[i] = dirSh.data[i] * radiance * (M_4PI / float(RAYS_PER_PROBE));

    [ForceUnroll]
    for (uint i = 0; i < SH_SIZE(SH_LEVEL); ++i)
    {
        __atomic_add(gSharedSH[i * 3 + 0], sh.data[i][0]);
        __atomic_add(gSharedSH[i * 3 + 1], sh.data[i][1]);
        __atomic_add(gSharedSH[i * 3 + 2], sh.data[i][2]);
    }

    GroupMemoryBarrierWithGroupSync();

    // Wave lanes are not synchronized after rayQuery, WTF? Have to use shared memory for accumulation

    /* sh.data[1] = float3(WaveActiveSum(1.0) / float(RAYS_PER_PROBE), 0, 0);
    sh.data[2] = float3(WaveGetLaneCount() / float(RAYS_PER_PROBE), 0, 0); */

    if (splatRayIdx == 0)
    {
        [ForceUnroll]
        for (uint i = 0; i < SH_SIZE(SH_LEVEL); ++i)
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
            SH<float3, SH_LEVEL> prevSh = shToFloat32(gPrevSplatProbes[splatIdx]);
            [ForceUnroll]
            for (uint i = 0; i < SH_SIZE(SH_LEVEL); ++i)
                sh.data[i] = lerp(sh.data[i], (float3)prevSh.data[i], 0.98);
        }

        if (error)
        {
            [ForceUnroll]
            for (uint i = 0; i < SH_SIZE(SH_LEVEL); ++i)
                sh.data[i] = float3(0, 1, 1);
        }

        gSplatProbes[splatIdx] = shToFloat16(sh);
    }
}
