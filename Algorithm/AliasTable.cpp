//
// Created by adamyuan on 12/16/24.
//

#include "AliasTable.hpp"

namespace GSGI
{

std::vector<uint64_t> AliasTable::normalizeWeights(std::span<const float> floatWeights)
{
    uint32_t count = floatWeights.size();
    double weightSum = 0;
    for (auto fltWeight : floatWeights)
    {
        FALCOR_ASSERT(fltWeight >= 0);
        weightSum += (double)fltWeight;
    }
    double multiplier = (double)0x100000000 * double(count) / weightSum;

    std::vector<uint64_t> weights(count);
    for (uint32_t i = 0; i < count; ++i)
        weights[i] = uint64_t(multiplier * (double)floatWeights[i]);

    return weights;
}

AliasTable AliasTable::create(std::span<const float> floatWeights)
{
    std::vector<uint64_t> weights = normalizeWeights(floatWeights);

    uint32_t count = floatWeights.size();
    std::vector<uint32_t> stack(count);
    uint32_t *const kStackBegin = stack.data(), *const kStackEnd = stack.data() + count;
    uint32_t *pSmall = kStackBegin, *pLarge = kStackEnd;

    // define bi-stack operations
#define SMALL_PUSH(x) (*(pSmall++) = (x))
#define LARGE_PUSH(x) (*(--pLarge) = (x))
#define SMALL_EMPTY (pSmall == kStackBegin)
#define LARGE_EMPTY (pLarge == kStackEnd)
#define SMALL_POP_AND_GET (*(--pSmall))
#define LARGE_POP_AND_GET (*(pLarge++))
#define LARGE_GET (*pLarge)
#define LARGE_POP (++pLarge)

    for (uint32_t i = 0; i < count; ++i)
    {
        if (weights[i] < 0x100000000)
            SMALL_PUSH(i);
        else
            LARGE_PUSH(i);
    }

    AliasTable table{};

    while (!SMALL_EMPTY && !LARGE_EMPTY)
    {
        uint32_t curSmall = SMALL_POP_AND_GET, curLarge = LARGE_GET;
        table.mEntries[curSmall].prob = weights[curSmall] & 0xFFFFFFFF;
        table.mEntries[curSmall].alias = curLarge;
        weights[curLarge] -= 0x100000000 - weights[curSmall];
        if (weights[curLarge] < 0x100000000)
        {
            LARGE_POP;
            SMALL_PUSH(curLarge);
        }
    }

    // Make invalid alias
    while (!LARGE_EMPTY)
    {
        uint32_t cur = LARGE_POP_AND_GET;
        table.mEntries[cur].alias = kNoAlias;
    }

    while (!SMALL_EMPTY)
    {
        uint32_t cur = SMALL_POP_AND_GET;
        table.mEntries[cur].alias = kNoAlias;
    }

#undef SMALL_PUSH
#undef LARGE_PUSH
#undef SMALL_EMPTY
#undef LARGE_EMPTY
#undef SMALL_POP_AND_GET
#undef LARGE_POP_AND_GET
#undef LARGE_GET
#undef LARGE_POP

    return table;
}

} // namespace GSGI
