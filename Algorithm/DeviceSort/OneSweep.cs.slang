#include "DeviceSorter.slangh"

#define WAVES_PER_GROUP (SORT_DIM / LANES_PER_WAVE)

globallycoherent RWStructuredBuffer<uint> gPassHists; // Size: divCeil(gKeyCount, SORT_PART_SIZE) * RADIX * PASS_COUNT
globallycoherent RWStructuredBuffer<uint> gIndices;   // Size: PASS_COUNT

StructuredBuffer<SortKey> gSrcKeys;
RWStructuredBuffer<SortKey> gDstKeys;

#if PAYLOAD_BUFFER_COUNT > 0
StructuredBuffer<SortPayload> gSrcPayloads[PAYLOAD_BUFFER_COUNT];
RWStructuredBuffer<SortPayload> gDstPayloads[PAYLOAD_BUFFER_COUNT];
#endif

uniform uint gPassIdx, gRadixShift, gPayloadBufferCount, gWriteKey;

// Shared Memory Layout:
// PHASE 1: |      WAVE_HIST      |GROUP_IDX|
// PHASE 2: |      WAVE_HIST      |                WAVE_MASK                |
// PHASE 3: |GROUP_OFST| WAVE_OFST|
// PHASE 4: |                      KEYS                      |  DEVICE_OFST |

#define SHARED_WAVE_HIST_SIZE (WAVES_PER_GROUP * RADIX)
#define SHARED_GROUP_IDX_SIZE (1)
#define SHARED_WAVE_MASK_SIZE (WAVES_PER_GROUP * RADIX * WAVE_MASK_SIZE)
#define SHARED_DEVICE_OFFSET_SIZE (RADIX)

#define SHARED_PHASE1_SIZE (SHARED_WAVE_HIST_SIZE + SHARED_GROUP_IDX_SIZE)
#define SHARED_PHASE2_SIZE (SHARED_WAVE_HIST_SIZE + SHARED_WAVE_MASK_SIZE)
#define SHARED_PHASE3_SIZE (SHARED_WAVE_HIST_SIZE)
#define SHARED_PHASE4_SIZE (SORT_PART_SIZE + SHARED_DEVICE_OFFSET_SIZE)
// SHARED_PHASE3_SIZE < SHARED_PHASE1_SIZE < SHARED_PHASE2_SIZE ? SHARED_PHASE4_SIZE

#define SHARED_PHASE1
#define SHARED_PHASE2
#define SHARED_PHASE3
#define SHARED_PHASE4

#if SHARED_PHASE2_SIZE > SHARED_PHASE4_SIZE
groupshared uint gShared[SHARED_PHASE2_SIZE];
#else
groupshared uint gShared[SHARED_PHASE4_SIZE];
#endif

#define SHARED_WAVE_HIST(WAVE_IDX, DIGIT) gShared[((WAVE_IDX) * RADIX) + (DIGIT)]
#define SHARED_GROUP_IDX SHARED_WAVE_HIST(WAVES_PER_GROUP, 0) // Not interfere with SHARED_WAVE_HIST
#define SHARED_WAVE_MASK(WAVE_IDX, DIGIT, UINT_IDX) \
    gShared[SHARED_WAVE_HIST_SIZE + ((WAVE_IDX) * WAVE_MASK_SIZE + (UINT_IDX)) * RADIX + (DIGIT)]
#define SHARED_WAVE_OFFSET(WAVE_IDX, DIGIT) SHARED_WAVE_HIST(WAVE_IDX, DIGIT) // WAVE_IDX >= 1
#define SHARED_GROUP_OFFSET(DIGIT) SHARED_WAVE_HIST(0, DIGIT)
#define SHARED_DEVICE_OFFSET(DIGIT) gShared[SORT_PART_SIZE + (DIGIT)]
#define SHARED_KEYS(IDX) gShared[IDX] // IDX < SORT_PART_SIZE

WaveMask sharedWaveMaskLoad(uint waveIdx, uint histIdx)
{
    WaveMask waveMask;
    [ForceUnroll]
    for (uint i = 0; i < WAVE_MASK_SIZE; ++i)
        waveMask[i] = SHARED_WAVE_MASK(waveIdx, histIdx, i);
    return waveMask;
}

void sharedWaveMaskStore(uint waveIdx, uint histIdx, WaveMask waveMask)
{
    [ForceUnroll]
    for (uint i = 0; i < WAVE_MASK_SIZE; ++i)
        SHARED_WAVE_MASK(waveIdx, histIdx, i) = waveMask[i];
}

#define PASS_HIST(PART_IDX, DIGIT) gPassHists[((gGroupCount * gPassIdx + (PART_IDX)) * RADIX) + (DIGIT)]
#define KEY_RADIX(KEY) extractKeyRadix((KEY), gRadixShift)

static uint gWaveLaneIdx, gGroupWaveIdx, gGroupThreadIdx, gGroupCount, gGroupIdx;
static WaveMask gWaveLtMask;

typedef uint16_t Offset_T; // Offset within the threadGroup

uint[SORT_KEYS_PER_THREAD] load(StructuredBuffer<uint> buffer, uint dummy)
{
    uint keys[SORT_KEYS_PER_THREAD];
    {
        uint firstLoadIdx = gGroupIdx * SORT_PART_SIZE + gGroupWaveIdx * LANES_PER_WAVE * SORT_KEYS_PER_THREAD + gWaveLaneIdx;
        if (gGroupIdx < gGroupCount - 1)
        {
            [ForceUnroll]
            for (uint i = 0, t = firstLoadIdx; i < SORT_KEYS_PER_THREAD; ++i, t += LANES_PER_WAVE)
                keys[i] = buffer[t];
        }
        else
        {
            [ForceUnroll]
            for (uint i = 0, t = firstLoadIdx; i < SORT_KEYS_PER_THREAD; ++i, t += LANES_PER_WAVE)
                keys[i] = t < gKeyCount ? buffer[t] : dummy;
        }
    }
    return keys;
}

SHARED_PHASE2 Offset_T[SORT_KEYS_PER_THREAD] matchKeys(in const SortKey keys[SORT_KEYS_PER_THREAD])
{
    Offset_T waveOffsets[SORT_KEYS_PER_THREAD];
    // Get offsets of a key among the keys with same value (wave domain)
    // Also generate a per-wave histogram in [gGroupWaveIdx * RADIX, gGroupWaveIdx * RADIX + RADIX)
    [ForceUnroll]
    for (uint i = 0; i < SORT_KEYS_PER_THREAD; ++i)
    {
        uint keyRadix = KEY_RADIX(keys[i]);

        // for example: 1 2 3 3 2
        // keyEqualMask for the 5 lanes are: 10000, 01001, 00110, 00110, 01001
        // lane with equal key is set to true.

#if 1
        // Faster match as described in
        // https://developer.download.nvidia.com/video/gputechconf/gtc/2020/presentations/s21572-a-faster-radix-sort-implementation.pdf

        // Clear needed wave mask
        sharedWaveMaskStore(gGroupWaveIdx, keyRadix, WaveMask(0));
        GroupMemoryBarrierWithWaveSync();

        // atomicOr wave mask
        InterlockedOr(SHARED_WAVE_MASK(gGroupWaveIdx, keyRadix, gWaveLaneIdx / 32), 1 << (gWaveLaneIdx & 31));
        GroupMemoryBarrierWithWaveSync();

        WaveMask keyEqualMask = sharedWaveMaskLoad(gGroupWaveIdx, keyRadix);
        uint keyLowestLaneIdx = waveMaskLSB(keyEqualMask);
        uint keyEqualLaneCount = waveMaskCountBits(keyEqualMask);
        uint keyEqualLtLaneCount = waveMaskCountBits(keyEqualMask & gWaveLtMask);

        uint keyOffset;
        if (gWaveLaneIdx == keyLowestLaneIdx)
            InterlockedAdd(SHARED_WAVE_HIST(gGroupWaveIdx, keyRadix), keyEqualLaneCount, keyOffset);
        waveOffsets[i] = Offset_T(WaveReadLaneAt(keyOffset, keyLowestLaneIdx) + keyEqualLtLaneCount);

#else

        WaveMask keyEqualMask = toWaveMask(uint4(-1, -1, -1, -1));
        [ForceUnroll]
        for (uint bitIdx = 0; bitIdx < BITS_PER_PASS; ++bitIdx)
        {
            bool bit = bool((keyRadix >> bitIdx) & 1);
            keyEqualMask &= toWaveMask(WaveActiveBallot(bit)) ^ WaveMask(bit ? 0 : -1);
        }

        // Index of first lane with equal keyRadix
        uint keyLowestLaneIdx = waveMaskLSB(keyEqualMask);

        uint keyEqualLaneCount = waveMaskCountBits(keyEqualMask);
        uint keyEqualLtLaneCount = waveMaskCountBits(keyEqualMask & gWaveLtMask);

        uint keyOffset;
        if (gWaveLaneIdx == keyLowestLaneIdx)
            InterlockedAdd(SHARED_WAVE_HIST(gGroupWaveIdx, keyRadix), keyEqualLaneCount, keyOffset);
        waveOffsets[i] = Offset_T(WaveReadLaneAt(keyOffset, keyLowestLaneIdx) + keyEqualLtLaneCount);

#endif
    }
    return waveOffsets;
}

SHARED_PHASE3 uint scanWaveHists()
{
    // Accumulate all per-wave histograms
    uint groupHist;
    if (gGroupThreadIdx < RADIX)
    {
        groupHist = SHARED_WAVE_HIST(0, gGroupThreadIdx);
        [ForceUnroll]
        for (uint i = 1; i < WAVES_PER_GROUP; ++i)
        {
            uint waveHist = SHARED_WAVE_HIST(i, gGroupThreadIdx);
            SHARED_WAVE_OFFSET(i, gGroupThreadIdx) = groupHist; // Exclusive Prefix-sum for Wave Hists
            groupHist += waveHist;
        }
    }
    return groupHist;
}

SHARED_PHASE3 uint scanGroupHist(uint groupHist)
{
    uint wavePrefix;

    if (gGroupThreadIdx < RADIX)
    {
        wavePrefix = WavePrefixSum(groupHist);
        // Directly write wave prefix sum at the final location
        if (gWaveLaneIdx == LANES_PER_WAVE - 1 && gGroupWaveIdx < WAVES_PER_GROUP)
            SHARED_GROUP_OFFSET(gGroupWaveIdx * LANES_PER_WAVE) = wavePrefix + groupHist;
    }
    GroupMemoryBarrierWithGroupSync();

    if (gGroupThreadIdx < (RADIX / LANES_PER_WAVE))
        SHARED_GROUP_OFFSET(gGroupThreadIdx * LANES_PER_WAVE) = WavePrefixSum(SHARED_GROUP_OFFSET(gGroupThreadIdx * LANES_PER_WAVE));

    GroupMemoryBarrierWithGroupSync();

    uint groupPrefix = 0;
    if (gGroupThreadIdx < RADIX)
    {
        groupPrefix = SHARED_GROUP_OFFSET(gGroupWaveIdx * LANES_PER_WAVE);
        if (gWaveLaneIdx > 0)
        {
            groupPrefix += wavePrefix;
            SHARED_GROUP_OFFSET(gGroupThreadIdx) = groupPrefix;
        }
    }

    return groupPrefix;
}

uint lookbackScanPassHists()
{
    if (gGroupThreadIdx < RADIX)
    {
        uint passHistPrefix = 0;
        for (uint part = gGroupIdx; part != -1;)
        {
            const uint flagPayload = PASS_HIST(part, gGroupThreadIdx);
            if ((flagPayload & FLAG_MASK) == FLAG_INCLUSIVE)
            {
                passHistPrefix += flagPayload >> 2;
                if (gGroupIdx < gGroupCount - 1)
                    InterlockedAdd(PASS_HIST(gGroupIdx + 1, gGroupThreadIdx), passHistPrefix << 2 | 1);

                return passHistPrefix;
            }

            if ((flagPayload & FLAG_MASK) == FLAG_REDUCTION)
            {
                passHistPrefix += flagPayload >> 2;
                --part;
            }
        }
    }
    return 0;
}

SHARED_PHASE4 void scatterShared(in const Offset_T groupOffsets[SORT_KEYS_PER_THREAD], in const uint keys[SORT_KEYS_PER_THREAD])
{
    [ForceUnroll]
    for (uint i = 0; i < SORT_KEYS_PER_THREAD; ++i)
        SHARED_KEYS(uint(groupOffsets[i])) = keys[i];
}

SHARED_PHASE4 void storeShared(RWStructuredBuffer<uint> buffer, in const uint deviceOffsets[SORT_KEYS_PER_THREAD])
{
    uint lastGroupKeyCount = gKeyCount - (gGroupCount - 1) * SORT_PART_SIZE;

    if (gGroupIdx < gGroupCount - 1)
        [ForceUnroll] for (uint i = 0, s = gGroupThreadIdx; i < SORT_KEYS_PER_THREAD; ++i, s += SORT_DIM) //
            buffer[deviceOffsets[i] + s] = SHARED_KEYS(s);
    else
        [ForceUnroll] for (uint i = 0, s = gGroupThreadIdx; i < SORT_KEYS_PER_THREAD; ++i, s += SORT_DIM)
        {
            if (s >= lastGroupKeyCount)
                break;
            buffer[deviceOffsets[i] + s] = SHARED_KEYS(s);
        }
}

[numthreads(SORT_DIM, 1, 1)]
void csMain(uint3 groupThreadID: SV_GroupThreadID)
{
    prepare();

    gWaveLaneIdx = WaveGetLaneIndex();
    gGroupWaveIdx = groupThreadID.x / LANES_PER_WAVE;
    gGroupThreadIdx = gGroupWaveIdx * LANES_PER_WAVE + gWaveLaneIdx;
    gWaveLtMask = getWaveLtMask(gWaveLaneIdx);
    gGroupCount = getSortPartCount();

    SHARED_PHASE1
    // Clear WAVE_HIST
    for (uint i = gGroupThreadIdx; i < SHARED_WAVE_HIST_SIZE; i += SORT_DIM)
        gShared[i] = 0;
    if (gGroupThreadIdx == 0)
        InterlockedAdd(gIndices[gPassIdx], 1, SHARED_GROUP_IDX);

    GroupMemoryBarrierWithGroupSync();
    gGroupIdx = SHARED_GROUP_IDX;

    SortKey keys[SORT_KEYS_PER_THREAD] = load(gSrcKeys, 0xFFFFFFFF);

    GroupMemoryBarrierWithGroupSync(); // End of SHARED_PHASE1

    SHARED_PHASE2
    // WAVE_HIST[0, ..., WAVES_PER_GROUP) are computed, along with waveOffsets
    Offset_T offsets[SORT_KEYS_PER_THREAD] = matchKeys(keys);

    GroupMemoryBarrierWithGroupSync(); // End of SHARED_PHASE2

    SHARED_PHASE3
    // WAVE_HIST[1, ..., WAVES_PER_GROUP) are scanned (exclusive prefix sum), WAVE_HIST[0] is left unused
    // WAVE_HIST[1, ..., WAVES_PER_GROUP) -> WAVE_OFFSET[1, ..., WAVES_PER_GROUP)
    uint groupHist = scanWaveHists();
    // Set FLAG_REDUCTION and data to next partition(group), for decoupled look-back
    if (gGroupThreadIdx < RADIX && gGroupIdx < gGroupCount - 1)
        InterlockedAdd(PASS_HIST(gGroupIdx + 1, gGroupThreadIdx), groupHist << 2 | FLAG_REDUCTION);

    GroupMemoryBarrierWithGroupSync(); // WAVE_HIST[0]/GROUP_OFFSET: READ -> READ/WRITE, WAVE_HIST[1..]/WAVE_OFFSET: WRITE -> READ

    // WAVE_HIST[0] -> GROUP_OFFSET
    uint groupHistPrefix = scanGroupHist(groupHist);
    GroupMemoryBarrierWithGroupSync(); // WAVE_HIST[0]/GROUP_OFFSET: WRITE -> READ

    // Adjust offsets
    // waveOffsets -> groupOffsets
    if (gGroupWaveIdx > 0)
        [ForceUnroll] for (uint i = 0; i < SORT_KEYS_PER_THREAD; ++i) //
            offsets[i] += Offset_T(SHARED_WAVE_OFFSET(gGroupWaveIdx, KEY_RADIX(keys[i])) + SHARED_GROUP_OFFSET(KEY_RADIX(keys[i])));
    else
        [ForceUnroll] for (uint i = 0; i < SORT_KEYS_PER_THREAD; ++i) //
            offsets[i] += Offset_T(SHARED_GROUP_OFFSET(KEY_RADIX(keys[i])));

    GroupMemoryBarrierWithGroupSync(); // End of SHARED_PHASE3

    SHARED_PHASE4
    // Scatter keys to shared memory (to make the final memory write ordered, utilizing GPU write coalescing)
    scatterShared(offsets, keys);
    uint passHistPrefix = lookbackScanPassHists();
    if (gGroupThreadIdx < RADIX)
        SHARED_DEVICE_OFFSET(gGroupThreadIdx) = passHistPrefix - groupHistPrefix;
    // minus groupHistPrefix so that SHARED_KEYS(s) -> DST(SHARED_DEVICE_OFFSET + s)

    GroupMemoryBarrierWithGroupSync(); // SHARED_KEYS, SHARED_DEVICE_OFFSET: WRITE -> READ

    // Remap and fetch deviceOffsets
    uint deviceOffsets[SORT_KEYS_PER_THREAD];
    [ForceUnroll]
    for (uint i = 0, s = gGroupThreadIdx; i < SORT_KEYS_PER_THREAD; ++i, s += SORT_DIM)
        deviceOffsets[i] = SHARED_DEVICE_OFFSET(KEY_RADIX(SHARED_KEYS(s)));

    if (bool(gWriteKey))
        storeShared(gDstKeys, deviceOffsets);

#if PAYLOAD_BUFFER_COUNT > 0
    for (uint i = 0; i < gPayloadBufferCount; ++i)
    {
        GroupMemoryBarrierWithGroupSync(); // SHARED_KEYS: READ -> WRITE
        scatterShared(offsets, load(gSrcPayloads[i], 0));
        GroupMemoryBarrierWithGroupSync(); // SHARED_KEYS: WRITE -> READ
        storeShared(gDstPayloads[i], deviceOffsets);
    }
#endif
    // End of SHARED_PHASE4
}
