#include "DeviceSorter.slangh"

StructuredBuffer<SortKey> gSortKeys;
RWStructuredBuffer<uint> gGlobalHists; // Size: RADIX * PASS_COUNT

#define RADIX_PER_GROUP 2

groupshared uint sShared[RADIX * RADIX_PER_GROUP][PASS_COUNT]; // Shared memory for GlobalHistogram

[numthreads(GLOBAL_HIST_DIM, 1, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    prepare();

    uint groupThreadIdx = groupThreadID.x, groupIdx = groupID.x;

    // clear shared memory
    for (uint i = groupThreadIdx; i < RADIX * RADIX_PER_GROUP; i += GLOBAL_HIST_DIM)
    {
        [ForceUnroll]
        for (uint s = 0; s < PASS_COUNT; ++s)
            sShared[i][s] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // Counting
    const uint histOffset = groupThreadIdx / (GLOBAL_HIST_DIM / RADIX_PER_GROUP) * RADIX;
    const uint partitionEnd = groupIdx == getGlobalHistPartCount() - 1 ? gKeyCount : (groupIdx + 1) * GLOBAL_HIST_PART_SIZE;
    SortKey key;
    for (uint i = groupIdx * GLOBAL_HIST_PART_SIZE + groupThreadIdx; i < partitionEnd; i += GLOBAL_HIST_DIM)
    {
        key = gSortKeys[i];
        [ForceUnroll]
        for (uint s = 0; s < PASS_COUNT; ++s)
            InterlockedAdd(sShared[extractKeyRadix(key, s * BITS_PER_PASS) + histOffset][s], 1);
    }

    GroupMemoryBarrierWithGroupSync();

    // Reduce and write
    for (uint i = groupThreadIdx; i < RADIX; i += GLOBAL_HIST_DIM)
    {
        [ForceUnroll]
        for (uint s = 0; s < PASS_COUNT; ++s)
        {
            uint sum = sShared[i][s];
            [ForceUnroll]
            for (uint r = 1; r < RADIX_PER_GROUP; ++r)
                sum += sShared[i + RADIX * r][s];

            InterlockedAdd(gGlobalHists[i + RADIX * s], sum);
        }
    }
}
