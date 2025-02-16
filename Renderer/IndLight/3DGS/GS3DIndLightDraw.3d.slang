import GaussianGI.Scene.GStaticScene;
import Utils.Math.FormatConversion;
import Utils.Math.Quaternion;
import GS3DIndLightSplat;
import GaussianGI.Renderer.GVBuffer;
#include "GaussianGI/Algorithm/Icosahedron.slangh"

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
RWStructuredBuffer<uint> gSplatIDs;
uniform float2 gResolution;
uniform GVBuffer gGVBuffer;

#define BOUND_FACTOR 3.3290429691304455 // \sqrt{2.0 * \ln(255)}

#define WRAP_GEOM_ICOSAHEDRON 0 // Must enable culling
#define WRAP_GEOM_BOX 1
#define WRAP_GEOM_QUAD 2 // Must disable culling

#define WRAP_GEOM WRAP_GEOM_QUAD

struct PSIn
{
    nointerpolation float3 splatMean;
    nointerpolation float16_t3 splatInvScale;
    nointerpolation float16_t4 splatInvRotate;
    nointerpolation float16_t3 indirect;
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
    o.splatMean = splat.mean;
    o.splatInvScale = float16_t3(1.0 / splat.scale);
    o.splatInvRotate = float16_t4(float4(-splat.rotate.xyz, splat.rotate.w) / dot(splat.rotate, splat.rotate));
    o.indirect = splat.indirect;

#if WRAP_GEOM == WRAP_GEOM_ICOSAHEDRON
    float4 clips[ICOSAHEDRON_VERTEX_COUNT];

    [ForceUnroll]
    for (uint i = 0; i < ICOSAHEDRON_VERTEX_COUNT; ++i)
    {
        float3 pos =
            splat.mean + BOUND_FACTOR / ICOSAHEDRON_FACE_DIST * rotate_vector(splat.scale * Icosahedron::kVertices[i], splat.rotate);
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

    float3 splatCamMean = splat.mean - gGStaticScene.camera.getPosition();
    float splatCamDist = length(splatCamMean);

    float3 splatViewDir = splatCamMean / splatCamDist;

    float3x3 splatRotMatT = float3x3(
        rotate_vector(float3(1, 0, 0), splat.rotate),
        rotate_vector(float3(0, 1, 0), splat.rotate),
        rotate_vector(float3(0, 0, 1), splat.rotate)
    );
    float3x3 splatRotMat = transpose(splatRotMatT);
    float3x3 splatScaleMat = float3x3(float3(splat.scale.x, 0, 0), float3(0, splat.scale.y, 0), float3(0, 0, splat.scale.z));

    float3 splatViewSideDir = select(
        all(splat.scale.x > splat.scale.yz), //
        splatRotMatT[0],
        select(splat.scale.y > splat.scale.z, splatRotMatT[1], splatRotMatT[2])
    );
    splatViewSideDir = normalize(cross(splatViewDir, splatViewSideDir));

    float3x3 splatVDMatT = float3x3(splatViewSideDir, cross(splatViewDir, splatViewSideDir), splatViewDir);
    float3x3 splatVDMat = transpose(splatVDMatT);

    float3 splatVDMean = float3(0, 0, splatCamDist);
    float3x3 splatVDRotScaleMat = mul(splatVDMatT, mul(splatRotMat, splatScaleMat));

    float3 splatVDExtent = float3(length(splatVDRotScaleMat[0]), length(splatVDRotScaleMat[1]), length(splatVDRotScaleMat[2]));

    float3x3 splatExtent = float3x3(splatVDMatT[0] * splatVDExtent.x, splatVDMatT[1] * splatVDExtent.y, splatVDMatT[2] * splatVDExtent.z);
    splatExtent *= BOUND_FACTOR;

    float3 splatQuadCenter = splat.mean - splatExtent[2];
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

    Ray ray = gGStaticScene.camera.computeRayPinhole(coord, uint2(gResolution), false);
    float3 o_g = rotate_vector(ray.origin - pIn.splatMean, pIn.splatInvRotate) * pIn.splatInvScale;
    float3 d_g = rotate_vector(ray.dir, pIn.splatInvRotate) * pIn.splatInvScale;
    float t_max = -dot(o_g, d_g) / dot(d_g, d_g); // 3D Gaussian Ray Tracing, 4.3 Evaluating Particle Response
    if (isinf(t_max) || isnan(t_max))
        discard;
    float3 x = ray.eval(t_max);
    float3 x_g = rotate_vector(x - pIn.splatMean, pIn.splatInvRotate) * pIn.splatInvScale;
    float power = max(dot(x_g, x_g), 0);
    float alpha = exp(-0.5 * power);

    var hitPoint = gGStaticScene.getHitPoint(hitInfo);
    // float maxDist = 16.0, dist = length(rotate_vector(hitPoint.position - pIn.splatMean, pIn.splatInvRotate) * pIn.splatInvScale);
    // TODO: Replace with ellipsoid distance
    float maxDist = 0.1, dist = distance(pIn.splatMean, hitPoint.position);
    float weight = 1.0 / clamp(dist / maxDist, 1e-6, 1.0) - 1.0;

    if (alpha < 1.0 / 255.0)
        discard;
    return float4(float3(pIn.indirect) * alpha, alpha) * weight;
}

