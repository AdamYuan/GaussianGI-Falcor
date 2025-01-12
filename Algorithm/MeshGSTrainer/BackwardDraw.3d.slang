import Utils.Math.FormatConversion;
import MeshGSTrainer;

#include "MeshGSTrainer.slangh"
#include "Utils/Math/MathConstants.slangh"

using namespace MeshGSTrainer;

SplatViewBuffer gSplatViews;
StructuredBuffer<uint> gSplatViewSortPayloads;
StructuredBuffer<float16_t2x2> gSplatViewAxes;
uniform float2 gResolution;

SplatTexture gDLossDCs_Ts;
CoherentRasterizerOrderedSplatTexture gRs_Ms;

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
    SplatView splatView = gSplatViews.load(splatViewID);
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
    float alpha = getAlphaSafe(pIn.splatView.geom, pIn.clipPos.xy, gResolution, shouldDiscard);

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

    SplatChannel ci = pIn.splatView.channel; // c_i

    float T;                                          // T = \prod (1 - \alpha_i)
    SplatChannel dL_dC = gDLossDCs_Ts.load(coord, T); // \dfrac{dL}{dC}

    beginInvocationInterlock();
    float Ri;                                          // R_i
    SplatChannel Mi = gRs_Ms.load(coord, Ri);          // M_i
    float Ri_1 = Ri * (1 - alpha);                     // R_{i - 1}
    SplatChannel Mi_1 = alpha * ci + (1 - alpha) * Mi; // M_{i - 1}
    gRs_Ms.store(coord, Mi_1, Ri_1);
    endInvocationInterlock();

    float Ti = T / Ri_1;

    SplatView dL_dsplatView;
    dL_dsplatView.channel = alpha * Ti * dL_dC;       // \dfrac{dL}{dc_i}
    float dL_dalpha = (Ti * (ci - Mi) * dL_dC).sum(); // \dfrac{dL}{d\alpha_i}
    {
        var p = diffPair(pIn.splatView.geom);
        bwd_diff(getAlpha)(p, pIn.clipPos.xy, gResolution, dL_dalpha);
        dL_dsplatView.geom = p.d;
    }
    if (shouldDiscard)
        dL_dsplatView = (SplatView)0;

    bool callAtomicAdd;
    [branch]
    if (WaveActiveAllEqual(pIn.splatViewID))
    {
        // Wave reduction
        callAtomicAdd = WaveIsFirstLane();
        dL_dsplatView = dL_dsplatView.waveActiveSum();
    }
    else
    {
        // Quad reduction
        uint waveLaneIdx = WaveGetLaneIndex();
        callAtomicAdd = waveLaneIdx == QuadReadLaneAt(waveLaneIdx, 0);
        dL_dsplatView = dL_dsplatView.quadSum();
    }

    if (callAtomicAdd)
        gDLossDSplatViews.atomicAdd(pIn.splatViewID, dL_dsplatView);
}

