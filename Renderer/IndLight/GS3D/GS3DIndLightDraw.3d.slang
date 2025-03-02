import Utils.Math.FormatConversion;
import Utils.Math.Quaternion;
import Utils.Math.MathHelpers;
import Utils.Sampling.TinyUniformSampleGenerator;
import GS3DIndLightSplat;
import GaussianGI.Scene.GStaticScene;
import GaussianGI.Renderer.GVBuffer;
import GaussianGI.Algorithm.SGGX;
import GaussianGI.Algorithm.SphericalHarmonics;
#include "Utils/Math/MathConstants.slangh"
#include "GaussianGI/Algorithm/Icosahedron.slangh"
#include "GaussianGI/Algorithm/GS3DBound.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
RWStructuredBuffer<uint> gSplatIDs;
RWBuffer<float> gSplatShadows;
uniform float2 gResolution;
uniform GVBuffer gGVBuffer;

StructuredBuffer<SH9<float3>> gSplatProbes;

#define BOUND_FACTOR GS3D_BOUND_SQRT_2LOG_255

#define WRAP_GEOM_ICOSAHEDRON 0 // Must enable culling
#define WRAP_GEOM_BOX 1
#define WRAP_GEOM_QUAD 2 // Must disable culling

#define WRAP_GEOM WRAP_GEOM_QUAD

struct PSIn
{
    nointerpolation float3 splatMean;
    nointerpolation float16_t3 splatInvScale;
    nointerpolation uint2 packedSplatInvRotate;
    nointerpolation float16_t3 albedo;
    nointerpolation float16_t shadow;
    // nointerpolation float16_t2x3 sggxM;
    nointerpolation float16_t3 normal;
    nointerpolation SH9<float3> probe;
    linear float4 clipPos : SV_Position;
}

uint vsMain(uint instanceID: SV_InstanceID)
{
    return instanceID;
}

#if WRAP_GEOM == WRAP_GEOM_ICOSAHEDRON
[maxvertexcount(ICOSAHEDRON_TRISTRIP_INDEX_COUNT)]
#elif WRAP_GEOM == WRAP_GEOM_QUAD
[maxvertexcount(4)]
#else
#error
#endif
void gsMain(point uint gIn[1], inout TriangleStream<PSIn> output)
{
    uint splatID = gSplatIDs[gIn[0]];
    var splat = gSplats.load(splatID);

    PSIn o;
    o.splatMean = splat.geom.mean;
    o.splatInvScale = float16_t3(1.0 / splat.geom.scale);
    o.albedo = float16_t3(splat.attrib.albedo);
    o.shadow = float16_t(gSplatShadows[splatID]);
    o.probe = gSplatProbes[splatID];
    /* [ForceUnroll]
    for (uint i = 0; i < o.probe.data.getCount(); ++i)
        o.probe.data[i] = float16_t3(1.0); */

    float3x3 splatRotMatT = float3x3(
        rotate_vector(float3(1, 0, 0), splat.geom.rotate),
        rotate_vector(float3(0, 1, 0), splat.geom.rotate),
        rotate_vector(float3(0, 0, 1), splat.geom.rotate)
    );
    float3x3 splatRotMat = transpose(splatRotMatT);

    /* SGGX::VNDFSampler vndfSampler =
        SGGX::VNDFSampler::fromSplat(splatRotMat, splat.geom.scale, normalize(gGStaticScene.camera.getPosition() - splat.geom.mean));
    o.sggxM = float16_t2x3(vndfSampler.M_kji); */
    o.normal =
        (float16_t3)SGGX::getSplatNormal(splatRotMat, splat.geom.scale, normalize(gGStaticScene.camera.getPosition() - splat.geom.mean));

    o.packedSplatInvRotate = uint2(packUnorm2x16(ndir_to_oct_unorm(splatRotMatT[0])), packUnorm2x16(ndir_to_oct_unorm(splatRotMatT[1])));

#if WRAP_GEOM == WRAP_GEOM_ICOSAHEDRON
    float4 clips[ICOSAHEDRON_VERTEX_COUNT];

    [ForceUnroll]
    for (uint i = 0; i < ICOSAHEDRON_VERTEX_COUNT; ++i)
    {
        float3 pos =
            splat.geom.mean + BOUND_FACTOR / ICOSAHEDRON_FACE_DIST * mul(splatRotMat, splat.geom.scale * Icosahedron::kVertices[i]);
        clips[i] = mul(gGStaticScene.camera.getViewProj(), float4(pos, 1.0));
    }

    [ForceUnroll]
    for (uint i = 0; i < ICOSAHEDRON_TRISTRIP_COUNT; ++i)
    {
        uint icoIdx = Icosahedron::kTriStrips[i];
        if (icoIdx == ICOSAHEDRON_TRISTRIP_RESTART)
            output.RestartStrip();
        else
        {
            o.clipPos = clips[icoIdx];
            output.Append(o);
        }
    }
#elif WRAP_GEOM == WRAP_GEOM_QUAD

    float3 splatCamMean = splat.geom.mean - gGStaticScene.camera.getPosition();
    float splatCamDist = length(splatCamMean);

    float3 splatViewDir = splatCamMean / splatCamDist;

    float3x3 splatScaleMat = float3x3(float3(splat.geom.scale.x, 0, 0), float3(0, splat.geom.scale.y, 0), float3(0, 0, splat.geom.scale.z));

    float3 splatViewSideDir = select(
        all(splat.geom.scale.x > splat.geom.scale.yz), //
        splatRotMatT[0],
        select(splat.geom.scale.y > splat.geom.scale.z, splatRotMatT[1], splatRotMatT[2])
    );
    splatViewSideDir = normalize(cross(splatViewDir, splatViewSideDir));

    float3x3 splatVDMatT = float3x3(splatViewSideDir, cross(splatViewDir, splatViewSideDir), splatViewDir);
    float3x3 splatVDMat = transpose(splatVDMatT);

    float3 splatVDMean = float3(0, 0, splatCamDist);
    float3x3 splatVDRotScaleMat = mul(splatVDMatT, mul(splatRotMat, splatScaleMat));

    float3 splatVDExtent = float3(length(splatVDRotScaleMat[0]), length(splatVDRotScaleMat[1]), length(splatVDRotScaleMat[2]));

    float3x3 splatExtent = float3x3(splatVDMatT[0] * splatVDExtent.x, splatVDMatT[1] * splatVDExtent.y, splatVDMatT[2] * splatVDExtent.z);
    splatExtent *= BOUND_FACTOR;

    float3 splatQuadCenter = splat.geom.mean - splatExtent[2];
    float4 splatClipQuadCenter = mul(gGStaticScene.camera.getViewProj(), float4(splatQuadCenter, 1));
    float2x4 splatClipExtentXY = float2x4(
        mul(gGStaticScene.camera.getViewProj(), float4(splatExtent[0], 0)),
        mul(gGStaticScene.camera.getViewProj(), float4(splatExtent[1], 0))
    );

    o.clipPos = splatClipQuadCenter - splatClipExtentXY[0] - splatClipExtentXY[1];
    output.Append(o);

    o.clipPos = splatClipQuadCenter + splatClipExtentXY[0] - splatClipExtentXY[1];
    output.Append(o);

    o.clipPos = splatClipQuadCenter - splatClipExtentXY[0] + splatClipExtentXY[1];
    output.Append(o);

    o.clipPos = splatClipQuadCenter + splatClipExtentXY[0] + splatClipExtentXY[1];
    output.Append(o);

    output.RestartStrip();
#else
#error
#endif
}

float4 psMain(PSIn pIn)
{
    uint2 coord = uint2(pIn.clipPos.xy);

    var hitInfo = gGVBuffer.LoadHitInfo(coord);
    if (!hitInfo.isHit)
        discard;

    float3x3 splatRotMatT = float3x3(
        oct_to_ndir_unorm(unpackUnorm2x16(pIn.packedSplatInvRotate.x)),
        oct_to_ndir_unorm(unpackUnorm2x16(pIn.packedSplatInvRotate.y)),
        float3(0, 0, 0)
    );
    splatRotMatT[2] = cross(splatRotMatT[0], splatRotMatT[1]);

    Ray ray = gGStaticScene.camera.computeRayPinhole(coord, uint2(gResolution), false);
    float3 o_g = mul(splatRotMatT, ray.origin - pIn.splatMean) * pIn.splatInvScale;
    float3 d_g = mul(splatRotMatT, ray.dir) * pIn.splatInvScale;
    float t_max = -dot(o_g, d_g) / dot(d_g, d_g); // 3D Gaussian Ray Tracing, 4.3 Evaluating Particle Response
    if (isinf(t_max) || isnan(t_max))
        discard;
    float3 x = ray.eval(t_max);
    float3 x_g = mul(splatRotMatT, x - pIn.splatMean) * pIn.splatInvScale;
    float power = max(dot(x_g, x_g), 0);
    float alpha = exp(-0.5 * power);

    var hitPoint = gGStaticScene.getHitPoint(hitInfo);
    float3 p = hitPoint.position - pIn.splatMean;

    // Ellipsoid SDF: https://iquilezles.org/articles/ellipsoids/
    /* float3 p_invScale = pIn.splatInvScale * 2.0;
    float3 p_g = mul(splatRotMatT, p);
    float k1 = length(p_g * p_invScale);
    float k2 = length(p_g * p_invScale * p_invScale);
    float maxDist = 0.1, dist = k1 * (k1 - 1.0) / k2; */

    float maxDist = 0.1, dist = length(p);

    // float maxDist = 0.1, dist = length(hitPoint.position - x);

    // float maxDist = 16.0, dist = length(mul(splatRotMatT, p) * pIn.splatInvScale);

    float weight = 1.0 / clamp(dist / maxDist, 1e-6, 1.0) - 1.0;

    if (alpha < 1.0 / 255.0)
        discard;

    /* SGGX::VNDFSampler vndfSampler =
        SGGX::VNDFSampler::create(float2x3(pIn.sggxM), normalize(gGStaticScene.camera.getPosition() - pIn.splatMean));

    TinyUniformSampleGenerator randGen = TinyUniformSampleGenerator(uint2(pIn.clipPos.xy), 0);
    float3 normal = vndfSampler.sample(sampleNext2D(randGen)); */

    // float3 splatNormal = float3(pIn.normal);

    // float3 color = splatNormal * 0.5 + 0.5; // float3(pIn.albedo * pIn.shadow);
    // if (any(isnan(color)) || any(isinf(color)))
    //     discard;

    float3 color = M_1_PI * max(shDiffuse(pIn.probe, shBasis<2>(hitPoint.normal)), 0.0);

    return float4(color * alpha, alpha) * weight;
}

