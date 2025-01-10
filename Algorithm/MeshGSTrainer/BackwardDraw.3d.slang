import Utils.Math.FormatConversion;
import MeshGSTrainer;

using namespace MeshGSTrainer;

SplatViewBuffer gSplatViews;
StructuredBuffer<uint> gSplatViewSortPayloads;
uniform float2 gCamResolution;

SplatTexture gDLossDCs_Ts;
CoherentRWSplatTexture gRs_Ms;

RWSplatViewBuffer gDLossDSplatViews;

struct PSIn
{
    nointerpolation uint splatViewID;
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
    SplatView splatView = gSplatViews.load(splatViewID);

    float2 clip = splatView.geom.clipMean;
    float2x2 deltaClip = getDeltaClip(splatView.geom, gCamResolution);

    PSIn o;
    o.splatViewID = splatViewID;
    o.splatView = splatView;
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

void psMain(PSIn pIn)
{
    float alpha = kOpacity * exp(-dot(pIn.quadPos, pIn.quadPos));
    if (alpha < kAlphaThreshold)
        discard;

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

    float dL_dalpha = (Ti * (ci - Mi) * dL_dC).sum();
    SplatView dL_dsplatView;
    dL_dsplatView.channel = alpha * Ti * dL_dC; // \dfrac{dL}{dc_i}
    {
        var p = diffPair(pIn.splatView.geom);
        bwd_diff(getAlpha)(p, pIn.clipPos.xy, gCamResolution, dL_dalpha);
        dL_dsplatView.geom = p.d;
    }
    gDLossDSplatViews.atomicAdd(pIn.splatViewID, dL_dsplatView);
}

