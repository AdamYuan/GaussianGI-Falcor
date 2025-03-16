import MeshGSTrainer;

using namespace MeshGSTrain;

uniform uint gSplatCount;
SplatBuffer gSplats;

RWSplatBuffer gDstSplats;

uniform uint gGrowCount, gActualGrowCount;
StructuredBuffer<uint> gGrowSplatIDs;
uniform uint gKeepCount, gActualKeepCount;
StructuredBuffer<uint> gKeepSplatIDs;

uniform float gSplitScaleThreshold;

[numthreads(64, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    uint srcSplatID;
    if (threadID.x < gKeepCount)
        srcSplatID = gKeepSplatIDs[threadID.x];
    else if (threadID.x < gKeepCount + gGrowCount)
        srcSplatID = gGrowSplatIDs[threadID.x - gKeepCount];
    else
        return;

    bool shouldKeep = threadID.x < gActualKeepCount;
    uint dstSplatID = select(shouldKeep, threadID.x, gActualKeepCount + (threadID.x - gActualKeepCount) * 2);

    var srcSplat = gSplats.load<Splat>(srcSplatID);

    if (shouldKeep)
    {
        gDstSplats.store(dstSplatID, srcSplat);
        return;
    }

    // Split or Clone
    gDstSplats.store(dstSplatID + 0, srcSplat);
    gDstSplats.store(dstSplatID + 1, srcSplat);
}

