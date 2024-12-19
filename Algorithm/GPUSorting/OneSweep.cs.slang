/******************************************************************************
 * GPUSorting
 * OneSweep Implementation
 *
 * SPDX-License-Identifier: MIT
 * Copyright Thomas Smith 4/28/2024
 * https://github.com/b0nes164/GPUSorting
 *
 * Based off of Research by:
 *          Andy Adinets, Nvidia Corporation
 *          Duane Merrill, Nvidia Corporation
 *          https://research.nvidia.com/publication/2022-06_onesweep-faster-least-significant-digit-radix-sort-gpus
 *
 ******************************************************************************/
#include "SweepCommon.slangh"

/* [numthreads(COPY_DIM, 1, 1)]
void csCopy(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= e_numKeys)
        return;

    b_alt[id.x] = b_sort[id.x];
    b_altPayload[id.x] = b_sortPayload[id.x];
} */

[numthreads(256, 1, 1)]
void csInitSweep(uint3 id: SV_DispatchThreadID)
{
    Prepare(PART_SIZE);

#if SORT_INDIRECT == 1
    if (id.x == 0)
    {
        uint numHistThreadBlocks = (e_numKeys + G_HIST_PART_SIZE - 1) / G_HIST_PART_SIZE;
        uint numSortThreadBlocks = e_threadBlocks; // (e_numKeys + PART_SIZE - 1) / PART_SIZE;
        // uint numCopyThreadBlocks = (e_numKeys + COPY_DIM - 1) / COPY_DIM;

        b_indirect[0] = numHistThreadBlocks;
        b_indirect[3] = numSortThreadBlocks;
    }
#endif
    const uint increment = 256 * 256;
    const uint clearEnd = e_threadBlocks * RADIX * RADIX_PASSES;
    for (uint i = id.x; i < clearEnd; i += increment)
        b_passHist[i] = 0;

    if (id.x < RADIX * RADIX_PASSES)
        b_globalHist[id.x] = 0;

    if (id.x < RADIX_PASSES)
        b_index[id.x] = 0;
}

[numthreads(G_HIST_DIM, 1, 1)]
void csGlobalHistogram(uint3 gtid: SV_GroupThreadID, uint3 gid: SV_GroupID)
{
    Prepare(G_HIST_PART_SIZE);

    // clear shared memory
    const uint histsEnd = RADIX * 2;
    for (uint i = gtid.x; i < histsEnd; i += G_HIST_DIM)
        g_gHist[i] = 0;
    GroupMemoryBarrierWithGroupSync();

    HistogramDigitCounts(gtid.x, gid.x);
    GroupMemoryBarrierWithGroupSync();

    ReduceWriteDigitCounts(gtid.x);
}

[numthreads(RADIX, 1, 1)]
void csScan(uint3 gtid: SV_GroupThreadID, uint3 gid: SV_GroupID)
{
    Prepare(PART_SIZE);

    LoadInclusiveScan(gtid.x, gid.x);
    GlobalHistExclusiveScanWGE16(gtid.x, gid.x);
}

[numthreads(D_DIM, 1, 1)]
void csDigitBinning(uint3 gtid: SV_GroupThreadID)
{
    Prepare(PART_SIZE);

    uint partitionIndex;
    KeyStruct keys;
    OffsetStruct offsets;

    // WGT 16 can potentially skip some barriers
    {
        if (WaveHistsSizeWGE16() < PART_SIZE)
            ClearWaveHists(gtid.x);

        AssignPartitionTile(gtid.x, partitionIndex);
        if (WaveHistsSizeWGE16() >= PART_SIZE)
        {
            GroupMemoryBarrierWithGroupSync();
            ClearWaveHists(gtid.x);
            GroupMemoryBarrierWithGroupSync();
        }
    }

    if (partitionIndex < e_threadBlocks - 1)
    {
        keys = LoadKeysWGE16(gtid.x, partitionIndex);
    }

    if (partitionIndex == e_threadBlocks - 1)
    {
        keys = LoadKeysPartialWGE16(gtid.x, partitionIndex);
    }

    uint exclusiveHistReduction;
    {
        offsets = RankKeysWGE16(gtid.x, keys);
        GroupMemoryBarrierWithGroupSync();

        uint histReduction;
        if (gtid.x < RADIX)
        {
            histReduction = WaveHistInclusiveScanCircularShiftWGE16(gtid.x);
            DeviceBroadcastReductionsWGE16(gtid.x, partitionIndex, histReduction);
            histReduction += WavePrefixSum(histReduction); // take advantage of barrier to begin scan
        }
        GroupMemoryBarrierWithGroupSync();

        WaveHistReductionExclusiveScanWGE16(gtid.x, histReduction);
        GroupMemoryBarrierWithGroupSync();

        UpdateOffsetsWGE16(gtid.x, offsets, keys);
        if (gtid.x < RADIX)
            exclusiveHistReduction = g_d[gtid.x]; // take advantage of barrier to grab value
        GroupMemoryBarrierWithGroupSync();
    }

    ScatterKeysShared(offsets, keys);
    Lookback(gtid.x, partitionIndex, exclusiveHistReduction);
    GroupMemoryBarrierWithGroupSync();

    if (partitionIndex < e_threadBlocks - 1)
        ScatterDevice(gtid.x, partitionIndex, offsets);

    if (partitionIndex == e_threadBlocks - 1)
        ScatterDevicePartial(gtid.x, partitionIndex, offsets);
}
