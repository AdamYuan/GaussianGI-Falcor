import Utils.Math.FormatConversion;
import GS3D;

RWStructuredBuffer<GS3D::SplatView> gSplatViews;
RWStructuredBuffer<uint> gSplatViewSortPayloads;
uniform float2 gResolution;
uniform float gAlpha, gD; // \alpha * \exp{-d^2} = \frac{1}{255} \Rightarrow d = \sqrt{\ln{255 \alpha}}

struct PSIn
{
    nointerpolation float3 color;
    linear float2 quadPos;
    linear float4 clipPos : SV_Position;
}

GS3D::SplatView vsMain(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    return gSplatViews[gSplatViewSortPayloads[instanceID]];
}

[maxvertexcount(4)]
void gsMain(point GS3D::SplatView gIn[1], uint instanceID: SV_InstanceID, inout TriangleStream<PSIn> output)
{
    GS3D::SplatView splatView = gIn[0];

    float2 clip = float2(splatView.clipXY);
    float2 deltaClip0 = float2(splatView.axis0) * 2 / gResolution.xy;
    float2 deltaClip1 = float2(splatView.axis1) * 2 / gResolution.xy;
    // deltaClip * 2 to scale from [0, 1] to [-1, 1]

    PSIn o;
    o.color = unpackUnorm3x8(splatView.color);

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

