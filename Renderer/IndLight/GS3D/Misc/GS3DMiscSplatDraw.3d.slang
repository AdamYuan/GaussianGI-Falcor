import GaussianGI.Scene.GStaticScene;
import Utils.Math.FormatConversion;
import GaussianGI.Renderer.IndLight.GS3D.GS3DIndLightSplat;
import GaussianGI.Algorithm.SphericalHarmonics;
import GS3DMiscColor;

using namespace GS3DIndLight;

uniform InstancedSplatBuffer gSplats;
StructuredBuffer<MiscSplatView> gSplatViews;
StructuredBuffer<uint> gSplatViewSortPayloads;
uniform float2 gResolution;
uniform float gAlpha, gD; // \alpha * \exp{-d^2} = \frac{1}{255} \Rightarrow d = \sqrt{\ln{255 \alpha}}

Buffer<float> gSplatShadows;
StructuredBuffer<SH<float3, 2>> gSplatProbes;

struct PSIn
{
    nointerpolation float3 color;
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
    o.color = getGS3DMiscColor(splat, gSplatShadows[splatID], gSplatProbes[splatID]);

    o.quadPos = float2(-gD, -gD);
    o.clipPos = float4(clip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1, 0, 1);
    output.Append(o);

    o.quadPos = float2(gD, -gD);
    o.clipPos = float4(clip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1, 0, 1);
    output.Append(o);

    o.quadPos = float2(-gD, gD);
    o.clipPos = float4(clip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1, 0, 1);
    output.Append(o);

    o.quadPos = float2(gD, gD);
    o.clipPos = float4(clip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1, 0, 1);
    output.Append(o);
}

float4 psMain(PSIn pIn)
{
    float alpha = gAlpha * exp(-dot(pIn.quadPos, pIn.quadPos));
    if (alpha < 1.0 / 255.0)
        discard;
    return float4(pIn.color, alpha);
}

