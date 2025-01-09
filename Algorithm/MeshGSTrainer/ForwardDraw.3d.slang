import Utils.Math.FormatConversion;
import MeshGSTrainer;

using namespace MeshGSTrainer;

SplatViewBuffer gSplatViews;
StructuredBuffer<uint> gSplatViewSortPayloads;
uniform float2 gCamResolution;

struct PSIn
{
    nointerpolation SplatChannel channel;
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
    o.channel = splatView.channel;
    o.quadPos = float2(-kQuadPosBound, -kQuadPosBound);
    o.clipPos = float4(clip + mul(o.quadPos, deltaClip), 0, 1);
    output.Append(o);
    o.quadPos = float2(kQuadPosBound, -kQuadPosBound);
    o.clipPos = float4(clip + mul(o.quadPos, deltaClip), 0, 1);
    output.Append(o);
    o.quadPos = float2(-kQuadPosBound, kQuadPosBound);
    o.clipPos = float4(clip + mul(o.quadPos, deltaClip), 0, 1);
    output.Append(o);
    o.quadPos = float2(kQuadPosBound, kQuadPosBound);
    o.clipPos = float4(clip + mul(o.quadPos, deltaClip), 0, 1);
    output.Append(o);
}

float4 psMain(PSIn pIn)
{
    float alpha = exp(-dot(pIn.quadPos, pIn.quadPos));
    if (alpha < kAlphaThreshold)
        discard;
    return float4(float3(pIn.channel.depth), alpha);
}

