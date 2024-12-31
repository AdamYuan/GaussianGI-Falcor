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

#define HISTS_PER_GROUP 2

#define SHARED_HIST_SIZE (HISTS_PER_GROUP * PASS_COUNT * RADIX)
#define SHARED_PASS_HIST(PASS_IDX, HIST_IDX, DIGIT) gShared[((PASS_IDX) * HISTS_PER_GROUP + (HIST_IDX)) * RADIX + (DIGIT)]

groupshared uint gShared[SHARED_HIST_SIZE]; // Shared memory for GlobalHistogram

[numthreads(GLOBAL_HIST_DIM, 1, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID)
{
    prepare();

    uint groupThreadIdx = groupThreadID.x, groupIdx = groupID.x, groupCount = divCeil(gKeyCount, GLOBAL_HIST_PART_SIZE);

    // Clear shared memory
    for (uint i = groupThreadIdx; i < SHARED_HIST_SIZE; i += GLOBAL_HIST_DIM)
        gShared[i] = 0;
    GroupMemoryBarrierWithGroupSync();

    // Counting
    SortKey keys[KEY_BUFFER_COUNT];
    for (uint i = groupIdx * GLOBAL_HIST_PART_SIZE + groupThreadIdx,
              histID = groupThreadIdx / (GLOBAL_HIST_DIM / HISTS_PER_GROUP),
              end = groupIdx == groupCount - 1 ? gKeyCount : (groupIdx + 1) * GLOBAL_HIST_PART_SIZE;
         i < end;
         i += GLOBAL_HIST_DIM)
    {
        [ForceUnroll]
        for (uint keyID = 0; keyID < KEY_BUFFER_COUNT; ++keyID)
            keys[keyID] = gSrcKeys[keyID][i];

        [ForceUnroll]
        for (uint passID = 0; passID < PASS_COUNT; ++passID)
        {
            var passDesc = getPassDesc(passID);
            uint radix = extractKeyRadix(keys[passDesc.keyID], passDesc.keyRadixShift);
            InterlockedAdd(SHARED_PASS_HIST(passID, histID, radix), 1);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Reduce and write
    for (uint i = groupThreadIdx; i < RADIX; i += GLOBAL_HIST_DIM)
    {
        [ForceUnroll]
        for (uint passID = 0; passID < PASS_COUNT; ++passID)
        {
            uint sum = SHARED_PASS_HIST(passID, 0, i);
            [ForceUnroll]
            for (uint histID = 1; histID < HISTS_PER_GROUP; ++histID)
                sum += SHARED_PASS_HIST(passID, histID, i);

            InterlockedAdd(gGlobalHists[RADIX * passID + i], sum);
        }
    }
}
