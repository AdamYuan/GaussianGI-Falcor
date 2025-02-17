import Utils.Math.FormatConversion;
import MeshGSTrainer;

using namespace MeshGSTrain;

SplatViewBuffer gSplatViews;
StructuredBuffer<uint> gSplatViewSortPayloads;
StructuredBuffer<float16_t2x2> gSplatViewAxes;
uniform float2 gResolution;
uniform float4x4 gCamInvProjMat;
uniform float gCamProjMat00;

struct PSIn
{
    nointerpolation SplatView splatView;
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
    var splatView = gSplatViews.load<SplatView>(splatViewID);
    float16_t2x2 splatViewAxis = gSplatViewAxes[splatViewID];

    float2 clip = splatView.geom.clipMean;
    float2x2 deltaClip = getDeltaClip(float2x2(splatViewAxis), gResolution);

    PSIn o;
    o.splatView = splatView;
    o.clipPos = float4(clip + mul(float2(-kQuadPosBound, -kQuadPosBound), deltaClip), 0, 1);
    output.Append(o);
    o.clipPos = float4(clip + mul(float2(kQuadPosBound, -kQuadPosBound), deltaClip), 0, 1);
    output.Append(o);
    o.clipPos = float4(clip + mul(float2(-kQuadPosBound, kQuadPosBound), deltaClip), 0, 1);
    output.Append(o);
    o.clipPos = float4(clip + mul(float2(kQuadPosBound, kQuadPosBound), deltaClip), 0, 1);
    output.Append(o);
}

Trait::SplatRTOut psMain(PSIn pIn)
{
    bool shouldDiscard;
    float alpha = getAlphaGSPPSafe(pIn.splatView.geom, pIn.clipPos.xy, gResolution, gCamProjMat00, gCamInvProjMat, shouldDiscard);
    // float alpha = getAlphaSafe(pIn.splatView.geom, pIn.clipPos.xy, gResolution, shouldDiscard);
    if (shouldDiscard)
        discard;
    return Trait::SplatRTOut::create(pIn.splatView.channel, alpha);
}

