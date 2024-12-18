//
// Created by adamyuan on 12/16/24.
//

#ifndef GSGI_ALIASTABLE_HPP
#define GSGI_ALIASTABLE_HPP

#include <concepts>
#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

class AliasTable
{
private:
    static constexpr uint32_t kNoAlias = 0xFFFFFFFF;
    struct Entry
    {
        uint32_t prob;
        uint32_t alias;
    };
    std::vector<Entry> mEntries;

    template<std::floating_point Float_T>
    static uint32_t toUnorm32(Float_T flt)
    {
        return (uint32_t)math::clamp(flt * (Float_T)0x100000000, (Float_T)0, (Float_T)0xFFFFFFFF);
    }
    static std::vector<uint64_t> normalizeWeights(std::span<const float> floatWeights);

public:
    static AliasTable create(std::span<const float> floatWeights);

    uint32_t getCount() const { return mEntries.size(); }

    uint32_t sample(uint32_t rand0, uint32_t rand1) const
    {
        uint32_t index = ((uint64_t)rand0 * getCount()) >> (uint64_t)32;
        auto entry = mEntries[index];
        if (rand1 >= entry.prob && entry.alias != kNoAlias)
            index = entry.alias;
        return index;
    }
    uint32_t sample(uint2 rand) const
    {
        static_assert(sizeof(rand.x) == sizeof(uint32_t));
        return sample(rand.x, rand.y);
    }
};

} // namespace GSGI

#endif // GSGI_ALIASTABLE_HPP
