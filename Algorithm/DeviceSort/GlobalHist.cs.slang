#include "DeviceSorter.slangh"

StructuredBuffer<SortKey> gSrcKeys[KEY_BUFFER_COUNT];
RWStructuredBuffer<uint> gGlobalHists; // Size: RADIX * PASS_COUNT

uniform uint gPassDescs[PASS_COUNT];
struct PassDesc
{
    uint keyID;
    uint keyRadixShift;
};

PassDesc getPassDesc(uint passID)
{
    uint packed = gPassDescs[passID];
    PassDesc desc;
    desc.keyID = packed >> 16;
    desc.keyRadixShift = packed & 0xFFFF;
    return desc;
}

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
        for (uint passID = 0; passID < PASS_COUNT; ++passID)
            sShared[i][passID] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // Counting
    const uint histOffset = groupThreadIdx / (GLOBAL_HIST_DIM / RADIX_PER_GROUP) * RADIX;
    const uint partitionEnd = groupIdx == getGlobalHistPartCount() - 1 ? gKeyCount : (groupIdx + 1) * GLOBAL_HIST_PART_SIZE;
    SortKey keys[KEY_BUFFER_COUNT];
    for (uint i = groupIdx * GLOBAL_HIST_PART_SIZE + groupThreadIdx; i < partitionEnd; i += GLOBAL_HIST_DIM)
    {
        [ForceUnroll]
        for (uint keyID = 0; keyID < KEY_BUFFER_COUNT; ++keyID)
            keys[keyID] = gSrcKeys[keyID][i];

        [ForceUnroll]
        for (uint passID = 0; passID < PASS_COUNT; ++passID)
        {
            var passDesc = getPassDesc(passID);
            InterlockedAdd(sShared[extractKeyRadix(keys[passDesc.keyID], passDesc.keyRadixShift) + histOffset][passID], 1);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Reduce and write
    for (uint i = groupThreadIdx; i < RADIX; i += GLOBAL_HIST_DIM)
    {
        [ForceUnroll]
        for (uint passID = 0; passID < PASS_COUNT; ++passID)
        {
            uint sum = sShared[i][passID];
            [ForceUnroll]
            for (uint r = 1; r < RADIX_PER_GROUP; ++r)
                sum += sShared[i + RADIX * r][passID];

            InterlockedAdd(gGlobalHists[i + RADIX * passID], sum);
        }
    }
}
