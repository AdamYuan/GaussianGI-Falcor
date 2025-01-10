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

uint vsMain(uint instanceID: SV_InstanceID)
{
    return instanceID;
}

[maxvertexcount(4)]
void gsMain(point uint gIn[1], inout TriangleStream<PSIn> output)
{
    SplatView splatView = gSplatViews.load(gSplatViewSortPayloads[gIn[0]]);

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

SplatRTOut psMain(PSIn pIn)
{
    float alpha = kOpacity * exp(-dot(pIn.quadPos, pIn.quadPos));
    if (alpha < kAlphaThreshold)
        discard;
    return SplatRTOut::create(pIn.channel.depth, alpha);
}

