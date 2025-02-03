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

Trait::SplatTexture gDLossDCs_Ts;
RWSplatViewBuffer gDLossDSplatViews;

#ifndef TRAIT_INCLUDE_PATH
#define TRAIT_INCLUDE_PATH "GaussianGI/Algorithm/MeshGSTrainer/Trait/Depth.slangh"
#endif

#define TRAIT_IN_BACKWARDDRAW_3D_SLANG
#include TRAIT_INCLUDE_PATH
#undef TRAIT_IN_BACKWARDDRAW_3D_SLANG

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

    float T;                                                 // T = \prod (1 - \alpha_i)
    Trait::SplatChannel dL_dC = gDLossDCs_Ts.load(coord, T); // \dfrac{dL}{dC}

    float oneMinusAlpha = 1 - alpha;
    Trait::SplatChannel ciMulAlpha = ci.mul(alpha);

    beginInvocationInterlock();
    float Ri;               // R_i
    Trait::SplatChannel Mi; // M_i
    coherentLoadR_M(coord, Ri, Mi);
    float Ri_1 = Ri * oneMinusAlpha;                                  // R_{i - 1}
    Trait::SplatChannel Mi_1 = ciMulAlpha.add(Mi.mul(oneMinusAlpha)); // M_{i - 1}
    coherentStoreR_M(coord, Ri_1, Mi_1);
    endInvocationInterlock();

    float Ti = T / Ri_1;

    SplatView dL_dsplatView;
    dL_dsplatView.channel = dL_dC.mul(Ti * alpha);         // \dfrac{dL}{dc_i}
    float dL_dalpha = dL_dC.mul(Ti).mul(ci.sub(Mi)).sum(); // \dfrac{dL}{d\alpha_i}
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

