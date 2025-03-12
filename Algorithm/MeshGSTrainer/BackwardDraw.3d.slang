import Utils.Math.FormatConversion;
import MeshGSTrainer;

#include "MeshGSTrainer.slangh"
#include "Utils/Math/MathConstants.slangh"

using namespace MeshGSTrain;

SplatViewBuffer gSplatViews;
StructuredBuffer<uint> gSplatViewSortPayloads;
StructuredBuffer<float16_t2x2> gSplatViewAxes;
uniform float2 gResolution;
uniform float4x4 gCamInvProjMat;
uniform float gCamProjMat00;

SplatTexture gDLossDCs_Ts;
CoherentSplatTexture gMs_Ts;
RWSplatViewBuffer gDLossDSplatViews;

struct PSIn
{
    nointerpolation uint splatViewID;
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
    o.splatViewID = splatViewID;
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

void psMain(PSIn pIn)
{
    bool shouldDiscard;
    float alpha = getAlphaGSPPSafe(pIn.splatView.geom, pIn.clipPos.xy, gResolution, gCamProjMat00, gCamInvProjMat, shouldDiscard);

    if (shouldDiscard)
    {
        if (WaveActiveAllEqual(pIn.splatViewID))
            discard;

        bool quadShouldDiscard = shouldDiscard;
        quadShouldDiscard = QuadReadAcrossX(quadShouldDiscard) & quadShouldDiscard;
        quadShouldDiscard = QuadReadAcrossY(quadShouldDiscard) & quadShouldDiscard;
        if (quadShouldDiscard) // All invocations in a quad should be discard, then discard
            discard;

        // Not all invocations in a quad should be discard, need to make the quad alive to enable latter quad reduction
        alpha = 0.0;
    }

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
    float Ti = Mi_Ti1.T * invOneMinusAlpha;                           // R_i
    Trait::SplatChannel Mi = Mi_Ti1.channel;                          // M_i
    Trait::SplatChannel Mi_1 = ciMulAlpha.add(Mi.mul(oneMinusAlpha)); // M_{i - 1}
    gMs_Ts.store(coord, SplatChannelT(Mi_1, Ti));
    endInvocationInterlock();

    SplatView dL_dsplatView;
    dL_dsplatView.channel = dL_dC.mul(Ti * alpha);                                     // \dfrac{dL}{dc_i}
    float dL_dalpha = dL_dC.mul(Ti).mul(ci.sub(Mi)).sum() - TdL_dT * invOneMinusAlpha; // \dfrac{dL}{d\alpha_i}
    var dp = diffPair(pIn.splatView.geom);
    bwd_diff(getAlphaGSPP)(dp, pIn.clipPos.xy, gResolution, gCamProjMat00, gCamInvProjMat, dL_dalpha);
    dL_dsplatView.geom = dp.d;
    if (shouldDiscard)
        dL_dsplatView = (SplatView)0;

    bool callAtomicAdd;
    [branch]
    if (WaveActiveAllEqual(pIn.splatViewID))
    {
        // Wave reduction
        callAtomicAdd = WaveIsFirstLane();
        dL_dsplatView = SplatView::fromSOA(dL_dsplatView.toSOA().waveActiveSum());
    }
    else
    {
        // Quad reduction
        uint waveLaneIdx = WaveGetLaneIndex();
        callAtomicAdd = waveLaneIdx == QuadReadLaneAt(waveLaneIdx, 0);
        dL_dsplatView = SplatView::fromSOA(dL_dsplatView.toSOA().quadSum());
    }

    if (callAtomicAdd)
        gDLossDSplatViews.atomicAdd(pIn.splatViewID, dL_dsplatView);
}

