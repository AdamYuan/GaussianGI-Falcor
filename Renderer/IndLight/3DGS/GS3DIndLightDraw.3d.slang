import GaussianGI.Scene.GStaticScene;
import Utils.Math.FormatConversion;
import Utils.Math.Quaternion;
import GS3DIndLightSplat;
import GaussianGI.Renderer.GVBuffer;

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
RWStructuredBuffer<SplatView> gSplatViews;
RWStructuredBuffer<uint> gSplatViewSortPayloads;
uniform float2 gResolution;
uniform GVBuffer gGVBuffer;

#define BOUND_FACTOR 2.3539888583335364

struct PSIn
{
    nointerpolation float3 splatMean;
    nointerpolation float16_t3 splatInvScale;
    nointerpolation float16_t4 splatInvRotate;
    nointerpolation float16_t3 indirect;
    linear float2 quadPos;
    linear float4 clipPos : SV_Position;
}

uint vsMain(uint instanceID: SV_InstanceID)
{
    return instanceID;
}

[maxvertexcount(4)]
void gsMain(point uint gIn[1], inout TriangleStream<PSIn> output)
{
    uint splatID = gSplatViewSortPayloads[gIn[0]];
    var splatView = gSplatViews[splatID];
    var splat = gSplats.load(splatID);

    float2 clip = float2(splatView.clipXY);
    float2 deltaClip0 = float2(splatView.axis0) * 2 / gResolution.xy;
    float2 deltaClip1 = float2(splatView.axis1) * 2 / gResolution.xy;
    // deltaClip * 2 to scale from [0, 1] to [-1, 1]

    PSIn o;
    o.splatMean = splat.mean;
    o.splatInvScale = float16_t3(1.0 / splat.scale);
    o.splatInvRotate = float16_t4(float4(-splat.rotate.xyz, splat.rotate.w) / dot(splat.rotate, splat.rotate));
    o.indirect = splat.indirect;

    o.quadPos = float2(-BOUND_FACTOR, -BOUND_FACTOR);
    o.clipPos = float4(clip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1, 0, 1);
    output.Append(o);

    o.quadPos = float2(BOUND_FACTOR, -BOUND_FACTOR);
    o.clipPos = float4(clip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1, 0, 1);
    output.Append(o);

    o.quadPos = float2(-BOUND_FACTOR, BOUND_FACTOR);
    o.clipPos = float4(clip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1, 0, 1);
    output.Append(o);

    o.quadPos = float2(BOUND_FACTOR, BOUND_FACTOR);
    o.clipPos = float4(clip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1, 0, 1);
    output.Append(o);
}

float4 psMain(PSIn pIn)
{
    uint2 coord = uint2(pIn.clipPos.xy);
    /* Ray ray = gGStaticScene.camera.computeRayPinhole(coord, uint2(gResolution), false);
    float3 o_g = rotate_vector(ray.origin - pIn.splatMean, pIn.splatInvRotate) * pIn.splatInvScale;
    float3 d_g = rotate_vector(ray.dir, pIn.splatInvRotate) * pIn.splatInvScale;
    float t_max = -dot(o_g, d_g) / dot(d_g, d_g); // 3D Gaussian Ray Tracing, 4.3 Evaluating Particle Response
    float3 x = ray.eval(t_max);

    float3 x_g = rotate_vector(x - pIn.splatMean, pIn.splatInvRotate) * pIn.splatInvScale; */
    // float alpha = exp(-0.5 * max(dot(x_g, x_g), 0));

    var hitInfo = gGVBuffer.LoadHitInfo(coord);
    var hitPoint = gGStaticScene.getHitPoint(hitInfo);

    float3 x_s = rotate_vector(hitPoint.position - pIn.splatMean, pIn.splatInvRotate) * pIn.splatInvScale;
    float opacity = 1.0 - smoothstep(10.0, 20.0, length(x_s));
    /* if (distance(hitPoint.position, pIn.splatMean) > 0.05)
        discard; */

    float alpha = exp(-0.25 * max(dot(x_s, x_s), 0));
    // float alpha = opacity * exp(-dot(pIn.quadPos, pIn.quadPos));
    if (alpha < 1.0 / 255.0)
        discard;
    return float4(float3(pIn.indirect), alpha);
}

