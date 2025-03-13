import Utils.Math.FormatConversion;
import MeshGSTrainer;

using namespace MeshGSTrain;

SplatViewBuffer gSplatViews;
SplatQuadBuffer gSplatQuads;
StructuredBuffer<uint> gSplatViewSortPayloads;
uniform float4x4 gCamProjMat, gCamInvProjMat;

CoherentSplatTexture gSplatRT;

struct PSIn
{
    nointerpolation SplatView splatView;
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
    var splatView = gSplatViews.load<SplatView>(splatViewID);
    var splatQuad = gSplatQuads.load<SplatQuad>(splatViewID);

    float3 center = SplatQuad::getCenter(splatView.geom.clipMean, gCamInvProjMat);
    float3 delta0 = splatQuad.delta0;
    float3 delta1 = splatQuad.delta1;

    float4 centerClip = mul(gCamProjMat, float4(center, 1));
    float4 deltaClip0 = mul(gCamProjMat, float4(delta0, 0));
    float4 deltaClip1 = mul(gCamProjMat, float4(delta1, 0));

    PSIn o;
    o.splatView = splatView;

    o.quadPos = float2(-kQuadPosBound, -kQuadPosBound);
    o.clipPos = centerClip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1;
    output.Append(o);

    o.quadPos = float2(kQuadPosBound, -kQuadPosBound);
    o.clipPos = centerClip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1;
    output.Append(o);

    o.quadPos = float2(-kQuadPosBound, kQuadPosBound);
    o.clipPos = centerClip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1;
    output.Append(o);

    o.quadPos = float2(kQuadPosBound, kQuadPosBound);
    o.clipPos = centerClip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1;
    output.Append(o);
    output.RestartStrip();
}

void psMain(PSIn pIn)
{
    float alpha = getAlpha(pIn.quadPos);
    if (alpha < kAlphaThreshold)
        discard;

    float oneMinusAlpha = 1.0 - alpha;

    uint2 coord = uint2(pIn.clipPos.xy);

    beginInvocationInterlock();
    var color = gSplatRT.load<SplatChannelT>(coord);
    color.channel = color.channel.mul(oneMinusAlpha).add(pIn.splatView.channel.mul(alpha));
    color.T *= oneMinusAlpha;
    gSplatRT.store(coord, color);
    endInvocationInterlock();
}

