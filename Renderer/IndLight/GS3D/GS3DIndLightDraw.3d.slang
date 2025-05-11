import Utils.Math.FormatConversion;
import Utils.Math.MathHelpers;
import Utils.Sampling.TinyUniformSampleGenerator;
import GS3DIndLightSplat;
import GaussianGI.Scene.GStaticScene;
import GaussianGI.Renderer.GVBuffer;
import GaussianGI.Algorithm.SGGX;
import GaussianGI.Algorithm.GS3D;
import GaussianGI.Algorithm.SphericalHarmonics;
#include "Utils/Math/MathConstants.slangh"
#include "GaussianGI/Algorithm/Icosahedron.slangh"
#include "GaussianGI/Algorithm/GS3DBound.slangh"

#include "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightSplatPrimitive.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
RWStructuredBuffer<uint> gSplatIDs;
RWBuffer<float> gSplatShadows;
uniform float2 gResolution;
uniform GVBuffer gGVBuffer;
Texture2D<uint2> gZNormals;

StructuredBuffer<SH9<float3>> gSplatProbes;

#ifndef USE_Z_NORMAL
#define USE_Z_NORMAL 1
#endif

#ifndef USE_DEPTH_GSPP
#define USE_DEPTH_GSPP 1
#endif

struct PSIn
{
    linear float2 quadPos;
    nointerpolation float3 splatMean;
    nointerpolation float splatMaxDist;
#if GS_PRIMITIVE_IS_RAY
    nointerpolation float2x3 splatCov3D;
#endif
    // nointerpolation float16_t3 splatScale;
    // nointerpolation uint2 packedSplatInvRotate;
    // nointerpolation float16_t3 albedo;
    // nointerpolation float16_t shadow;
    // nointerpolation float16_t3 normal;
    nointerpolation SH9<float3> probe;
    linear float4 clipPos : SV_Position;
}

uint vsMain(uint instanceID: SV_InstanceID)
{
    return instanceID;
}

[maxvertexcount(4)]
void gsMain(point uint gIn[1], inout TriangleStream<PSIn> output)
{
#if USE_DEPTH_GSPP
    uint splatID = gSplatIDs[gSplats.splatCount - 1u - gIn[0]];
#else
    uint splatID = gSplatIDs[gIn[0]];
#endif
    var splat = gSplats.load(splatID);
    var splatGeom = splat.geom;

    PSIn o;
    o.splatMaxDist = max(splat.geom.scale.x, max(splat.geom.scale.y, splat.geom.scale.z)) * BOUND_FACTOR;

#if USE_DEPTH_GSPP
    float3 splatCamMean = splatGeom.mean - gGStaticScene.camera.getPosition();
    float splatCamDist = length(splatCamMean);
    GSPPPrimitive gsPrim = GSPPPrimitive::create(splatGeom, gGStaticScene.camera, gResolution, splatCamDist - o.splatMaxDist);
#else
    GSPrimitive gsPrim = GSPrimitive::create(splatGeom, gGStaticScene.camera, gResolution);
#endif

    o.splatMean = splat.geom.mean;
    // o.splatScale = float16_t3(splat.geom.scale);
    // o.albedo = float16_t3(splat.attrib.albedo);
    // o.shadow = float16_t(gSplatShadows[splatID]);
    o.probe = shConvolve(gSplatProbes[splatID], shDiffuseLobe<2>());
    /* [ForceUnroll]
    for (uint i = 0; i < 9; ++i)
        o.probe.data[i] = float16_t3(1.0); */

#if GS_PRIMITIVE_IS_RAY
    o.splatCov3D = gsPrim.splatCov3D;
#endif

    [ForceUnroll]
    for (uint i = 0; i < 4; ++i)
    {
        o.quadPos = gsPrim.getVertexQuadPos(i);
        o.clipPos = gsPrim.getVertexClipPos(i);
        output.Append(o);
    }
    output.RestartStrip();
}

[earlydepthstencil]
float4 psMain(PSIn pIn)
{
    uint2 coord = uint2(pIn.clipPos.xy);

#if GS_PRIMITIVE_IS_RAY
    float alpha = GSPrimitive::getAlpha(gGStaticScene.camera, uint2(gResolution), uint2(pIn.clipPos.xy), pIn.splatCov3D, pIn.splatMean);
#else
    float alpha = GSPrimitive::getAlpha(pIn.quadPos);
#endif

    if (alpha < 1.0 / 255.0)
        discard;

    float3 normal, position;

#if USE_Z_NORMAL
    uint2 zNormal = gZNormals[coord];
    normal = oct_to_ndir_unorm(unpackUnorm2x16(zNormal.y));
    float z = asfloat(zNormal.x);
    float4 invClip = float4((pIn.clipPos.xy / gResolution), z, 1.0);
    invClip.xy = invClip.xy * 2.0 - 1.0;
    invClip.y = -invClip.y;
    invClip = mul(gGStaticScene.camera.data.invViewProj, invClip);
    position = invClip.xyz / invClip.w;
#else
    var hitInfo = gGVBuffer.LoadHitInfo(coord);
    var hitPoint = gGStaticScene.getHitPoint(hitInfo);
    normal = hitPoint.normal;
    position = hitPoint.position;
#endif

    float3 p = position - pIn.splatMean;
    float maxDist = pIn.splatMaxDist, dist = length(p);
    float weight = 1.0 / clamp(dist / maxDist, 1e-6, 1.0) - 1.0;

    float3 color = M_1_PI * max(shDot(pIn.probe, shBasis<2>(normal)), 0.0);

    return float4(color * alpha, alpha) * weight;
}

