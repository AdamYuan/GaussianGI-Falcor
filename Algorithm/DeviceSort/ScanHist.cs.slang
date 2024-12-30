#include "DeviceSorter.slangh"

StructuredBuffer<uint> gGlobalHists; // Size: RADIX * PASS_COUNT
RWStructuredBuffer<uint> gPassHists; // Size: divCeil(gKeyCount, SORT_PART_SIZE) * RADIX * PASS_COUNT

groupshared uint sShared[RADIX / LANES_PER_WAVE];

// Exclusive prefix sum for Histograms

[numthreads(RADIX, 1, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    prepare();

    uint waveLaneIdx = WaveGetLaneIndex(), groupWaveIdx = groupThreadID.x / LANES_PER_WAVE, //
        groupThreadIdx = groupWaveIdx * LANES_PER_WAVE + waveLaneIdx, groupIdx = groupID.x;

    uint hist = gGlobalHists[groupIdx * RADIX + groupThreadIdx];
    uint wavePrefixSum = WavePrefixSum(hist);

    if (waveLaneIdx == LANES_PER_WAVE - 1)
        sShared[groupWaveIdx] = wavePrefixSum + hist;

    GroupMemoryBarrierWithGroupSync();

#if RADIX / LANES_PER_WAVE > LANES_PER_WAVE
#error LANES_PER_WAVE too small
#endif
    if (groupThreadIdx < (RADIX / LANES_PER_WAVE))
        sShared[groupThreadIdx] = WavePrefixSum(sShared[groupThreadIdx]);

    GroupMemoryBarrierWithGroupSync();

    uint prefixSum = wavePrefixSum + sShared[groupWaveIdx];
    gPassHists[groupIdx * RADIX * getSortPartCount() + groupThreadIdx] = (prefixSum << 2) | FLAG_INCLUSIVE;
}
