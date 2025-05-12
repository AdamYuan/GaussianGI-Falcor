import Utils.Math.FormatConversion;
import MeshGSTrainer;

#include "MeshGSTrainer.slangh"
#include "Utils/Math/MathConstants.slangh"

using namespace MeshGSTrain;

SplatViewBuffer gSplatViews;
SplatQuadBuffer gSplatQuads;
StructuredBuffer<uint> gSplatViewSortPayloads;
uniform float4x4 gCamProjMat;

SplatTexture gDLossDCs_Ts;
CoherentSplatTexture gMs_Ts;
RWSplatViewBuffer gDLossDSplatViews;

struct PSIn
{
    nointerpolation uint splatViewID;
    nointerpolation SplatView splatView;
    linear float3 viewPos;
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

    float3 centerView = splatView.geom.viewDir;
    float3 deltaView0 = splatQuad.deltaView0;
    float3 deltaView1 = splatQuad.deltaView1;

    float4 centerClip = mul(gCamProjMat, float4(centerView, 1));
    float4 deltaClip0 = mul(gCamProjMat, float4(deltaView0, 0));
    float4 deltaClip1 = mul(gCamProjMat, float4(deltaView1, 0));

    PSIn o;
    o.splatViewID = splatViewID;
    o.splatView = splatView;

    o.quadPos = float2(-kQuadPosBound, -kQuadPosBound);
    o.viewPos = centerView + o.quadPos.x * deltaView0 + o.quadPos.y * deltaView1;
    o.clipPos = centerClip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1;
    output.Append(o);

    o.quadPos = float2(kQuadPosBound, -kQuadPosBound);
    o.viewPos = centerView + o.quadPos.x * deltaView0 + o.quadPos.y * deltaView1;
    o.clipPos = centerClip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1;
    output.Append(o);

    o.quadPos = float2(-kQuadPosBound, kQuadPosBound);
    o.viewPos = centerView + o.quadPos.x * deltaView0 + o.quadPos.y * deltaView1;
    o.clipPos = centerClip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1;
    output.Append(o);

    o.quadPos = float2(kQuadPosBound, kQuadPosBound);
    o.viewPos = centerView + o.quadPos.x * deltaView0 + o.quadPos.y * deltaView1;
    o.clipPos = centerClip + o.quadPos.x * deltaClip0 + o.quadPos.y * deltaClip1;
    output.Append(o);
    output.RestartStrip();
}

void psMain(PSIn pIn)
{
    float alpha = getAlpha(pIn.quadPos);
    bool cullingFlag = alpha < kAlphaThreshold || IsHelperLane();

    if (QuadAll(cullingFlag)) // All invocations in a quad should be discard, then discard
        discard;

    uint2 coord = uint2(pIn.clipPos.xy);

    Trait::SplatChannel ci = pIn.splatView.channel; // c_i

    var dLdC_TdLdT = gDLossDCs_Ts.load<SplatChannelT>(coord);

    Trait::SplatChannel dL_dC = dLdC_TdLdT.channel; // \dfrac{dL}{dC}
    float TdL_dT = dLdC_TdLdT.T;                    // T \dfrac{dL}{dT}

    float oneMinusAlpha = 1 - alpha;
    float invOneMinusAlpha = 1.0 / oneMinusAlpha;
    Trait::SplatChannel ciMulAlpha = ci.mul(alpha);

    beginInvocationInterlock();
    var Mi_Ti1 = gMs_Ts.load<SplatChannelT>(coord);                   // M_i & T_{i + 1}
    float Ti = Mi_Ti1.T * invOneMinusAlpha;                           // T_i
    Trait::SplatChannel Mi = Mi_Ti1.channel;                          // M_i
    Trait::SplatChannel Mi_1 = ciMulAlpha.add(Mi.mul(oneMinusAlpha)); // M_{i - 1}
    gMs_Ts.store(coord, SplatChannelT(Mi_1, Ti));
    endInvocationInterlock();

    Trait::SplatChannel dL_dci = dL_dC.mul(Ti * alpha);                                // \dfrac{dL}{dc_i}
    float dL_dalpha = dL_dC.mul(Ti).mul(ci.sub(Mi)).sum() - TdL_dT * invOneMinusAlpha; // \dfrac{dL}{d\alpha_i}
    var dp = diffPair(pIn.splatView.geom);
    bwd_diff(getAlpha)(dp, pIn.viewPos, dL_dalpha);

    FlatSplatView dL_dsplatView = SplatView(dp.d, dL_dci).toSOA();

    bool reduceFlag = !cullingFlag;
    if (dL_dsplatView.anyIsNanOrInf())
    {
        dL_dsplatView = (FlatSplatView)0;
        reduceFlag = false;
    }

    // HLSL sucks. Fuck HLSL. HLSL <<<<<<<<< GLSL
    WaveMask reduceMask = WaveActiveBallot(reduceFlag).x;
    uint waveLaneIdx = WaveGetLaneIndex();

    [branch]
    if (WaveActiveAllEqual(pIn.splatViewID) && countbits(reduceMask) >= 16)
    {
        // Wave reduction
        dL_dsplatView = dL_dsplatView.waveActiveSum();
    }
    else
    {
        // Quad reduction
        dL_dsplatView = dL_dsplatView.quadSum();
        reduceMask &= (1 << waveLaneIdx);
        reduceMask |= QuadReadAcrossX(reduceMask);
        reduceMask |= QuadReadAcrossY(reduceMask);
    }

    if (waveLaneIdx == firstbitlow(reduceMask))
        gDLossDSplatViews.atomicAdd(pIn.splatViewID, dL_dsplatView);
}

