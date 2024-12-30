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

#if SORT_INDIRECT == 1
    if (threadID.x == 0)
    {
        gIndirectArgs[0] = getGlobalHistPartCount();
        gIndirectArgs[3] = getSortPartCount();
    }
#endif
    const uint increment = 256 * 256;
    const uint clearEnd = getSortPartCount() * PASS_COUNT * RADIX;
    for (uint i = threadID.x; i < clearEnd; i += increment)
        gPassHists[i] = 0;

    if (threadID.x < PASS_COUNT * RADIX)
        gGlobalHists[threadID.x] = 0;

    if (threadID.x < PASS_COUNT)
        gIndices[threadID.x] = 0;
}
