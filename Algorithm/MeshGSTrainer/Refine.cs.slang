import MeshGSTrainer;
import GaussianGI.Algorithm.GS3D;

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

    float3 scale = abs(srcSplat.geom.scale);
    float3 mean = srcSplat.geom.mean;
    float3x3 rotMat = GS3D::getQuatMatrix(srcSplat.geom.rotate);

    uint mainAxis = select(all(scale.z > scale.xy), 2, uint(scale.y > scale.x));
    float mainScale = scale[mainAxis]; // Dynamic indexing is not good, but we don't care

    if (mainScale > gSplitScaleThreshold)
        srcSplat.geom.scale = scale * 0.5; // Reduce scale if split

    float3 deltaMean = transpose(rotMat)[mainAxis] * mainScale;

    srcSplat.geom.mean = mean + deltaMean;
    gDstSplats.store(dstSplatID + 0, srcSplat);
    srcSplat.geom.mean = mean - deltaMean;
    gDstSplats.store(dstSplatID + 1, srcSplat);
}

