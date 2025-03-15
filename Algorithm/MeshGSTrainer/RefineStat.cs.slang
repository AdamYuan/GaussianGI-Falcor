import MeshGSTrainer;
import GaussianGI.Algorithm.ShaderMisc;

using namespace MeshGSTrain;

uniform uint gSplatCount;
SplatBuffer gSplats;

RWStructuredBuffer<uint> gGrowCount, gGrowSortKeys, gGrowSortPayloads;
RWStructuredBuffer<uint> gKeepCount, gKeepSplatIDs;

RWStructuredBuffer<float> gAccumGrads;
RWStructuredBuffer<uint> gAccumViews;

uniform float gGrowGradThreshold;

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint splatID = threadID.x;
    if (splatID >= gSplatCount)
        return;

    var splat = gSplats.load<Splat>(splatID);

    float accumGrad = gAccumGrads[splatID];
    uint accumView = gAccumViews[splatID];
    float avgGrad = accumGrad / float(accumView);
    // Clear accum buffers
    gAccumGrads[splatID] = 0.0;
    gAccumViews[splatID] = 0;

    // Prune
    if (Trait::prune(splat.geom, splat.attrib))
        return;

    if (accumView > 0 && avgGrad >= gGrowGradThreshold)
    {
        // Grow
        uint growID;
        InterlockedAdd(gGrowCount[0], 1u, growID);
        gGrowSortKeys[growID] = float2CmpUint(avgGrad); // Small -> Large
        gGrowSortPayloads[growID] = splatID;
    }
    else
    {
        // Keep
        uint keepID;
        InterlockedAdd(gKeepCount[0], 1u, keepID);
        gKeepSplatIDs[keepID] = splatID;
    }
}

