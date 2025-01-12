import Utils.Math.FormatConversion;
import MeshGSTrainer;

using namespace MeshGSTrainer;

SplatViewBuffer gSplatViews;
StructuredBuffer<uint> gSplatViewSortPayloads;
StructuredBuffer<float16_t2x2> gSplatViewAxes;
uniform float2 gResolution;

struct PSIn
{
    nointerpolation SplatChannel channel;
    nointerpolation SplatViewGeom geom;
    linear float2 quadPos;
    linear float4 clipPos : SV_Position;
};

uint vsMain(uint instanceID: SV_InstanceID)
{
    return instanceID;
}

[maxvertexcount(4)]
void gsMain(point uint gIn[1], inout TriangleStream<PSIn> output)
{
    uint splatViewID = gSplatViewSortPayloads[gIn[0]];
    SplatView splatView = gSplatViews.load(splatViewID);
    float16_t2x2 splatViewAxis = gSplatViewAxes[splatViewID];

    float2 clip = splatView.geom.clipMean;
    float2x2 deltaClip = getDeltaClip(float2x2(splatViewAxis), gResolution);

    PSIn o;
    o.channel = splatView.channel;
    o.geom = splatView.geom;
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

