#include "DeviceSorter.slangh"

#if SORT_INDIRECT == 1
RWStructuredBuffer<uint> gIndirectArgs;
#endif

RWStructuredBuffer<uint> gGlobalHists; // Size: RADIX * PASS_COUNT
RWStructuredBuffer<uint> gPassHists;   // Size: divCeil(gKeyCount, SORT_PART_SIZE) * RADIX * PASS_COUNT
RWStructuredBuffer<uint> gIndices;     // Size: PASS_COUNT

// 256 * 256 threads
[numthreads(256, 1, 1)]
void csMain(uint3 threadID: SV_DispatchThreadID)
{
    prepare();

    uint threadIdx = threadID.x;

    uint sortGroupCount = getSortPartCount();

#if SORT_INDIRECT == 1
    if (threadIdx == 0)
    {
        gIndirectArgs[0] = divCeil(gKeyCount, GLOBAL_HIST_PART_SIZE);
        gIndirectArgs[3] = sortGroupCount;
    }
#endif
    for (uint i = threadIdx, end = sortGroupCount * PASS_COUNT * RADIX; i < end; i += 256 * 256)
        gPassHists[i] = 0;

    if (threadIdx < PASS_COUNT * RADIX)
        gGlobalHists[threadIdx] = 0;

    if (threadIdx < PASS_COUNT)
        gIndices[threadIdx] = 0;
}
