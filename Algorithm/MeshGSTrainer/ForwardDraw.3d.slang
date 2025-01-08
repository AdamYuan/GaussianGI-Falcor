import Utils.Math.FormatConversion;
#include "Trainer.slangh"

SplatViewBuffer gSplatViews;
StructuredBuffer<uint> gSplatViewSortPayloads;
uniform float2 gCamResolution;

struct PSIn
{
    nointerpolation SplatViewColor color;
    linear float2 quadPos;
    linear float4 clipPos : SV_Position;
}

SplatView vsMain(uint instanceID: SV_InstanceID)
{
    return gSplatViews.load(gSplatViewSortPayloads[instanceID]);
}

[maxvertexcount(4)]
void gsMain(point SplatView gIn[1], uint instanceID: SV_InstanceID, inout TriangleStream<PSIn> output)
{
    SplatView splatView = gIn[0];

    float2 clip = splatView.geom.clipMean;
    float2x2 deltaClip = getDeltaClip(splatView.geom, gCamResolution);

    PSIn o;
    o.color = splatView.color;
    o.quadPos = float2(-QUAD_POS_BOUND, -QUAD_POS_BOUND);
    o.clipPos = float4(clip + mul(o.quadPos, deltaClip), 0, 1);
    output.Append(o);
    o.quadPos = float2(QUAD_POS_BOUND, -QUAD_POS_BOUND);
    o.clipPos = float4(clip + mul(o.quadPos, deltaClip), 0, 1);
    output.Append(o);
    o.quadPos = float2(-QUAD_POS_BOUND, QUAD_POS_BOUND);
    o.clipPos = float4(clip + mul(o.quadPos, deltaClip), 0, 1);
    output.Append(o);
    o.quadPos = float2(QUAD_POS_BOUND, QUAD_POS_BOUND);
    o.clipPos = float4(clip + mul(o.quadPos, deltaClip), 0, 1);
    output.Append(o);
}

float4 psMain(PSIn pIn)
{
    float alpha = exp(-dot(pIn.quadPos, pIn.quadPos));
    if (alpha < ALPHA_THRESHOLD)
        discard;
    return float4(float3(pIn.color.depth), alpha);
}

