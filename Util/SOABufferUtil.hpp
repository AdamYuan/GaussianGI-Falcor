//
// Created by adamyuan on 1/14/25.
//

#ifndef GSGI_SOABUFFERUTIL_HPP
#define GSGI_SOABUFFERUTIL_HPP

#include <Falcor.h>
#include <ranges>

using namespace Falcor;

namespace GSGI
{

template<typename Word_T, uint32_t WordsPerUnit_V>
struct SOAUnitTrait
{
    static_assert(sizeof(Word_T) == sizeof(uint32_t));
    static_assert(WordsPerUnit_V == 1 || WordsPerUnit_V == 2 || WordsPerUnit_V == 4);
};

template<typename, uint32_t>
struct SOATrait;

template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
struct SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>
{
    static_assert(WordsPerElem_V > WordsPerUnit_V);
    static constexpr uint32_t kWordsPerElem = WordsPerElem_V;
    static constexpr uint32_t kUnitsPerElem = WordsPerElem_V / WordsPerUnit_V - (WordsPerElem_V % WordsPerUnit_V == 0);
    static constexpr uint32_t kWordsPerExt = WordsPerElem_V - kUnitsPerElem * WordsPerUnit_V;
    static constexpr uint32_t kPaddedWordsPerExt = kWordsPerExt == 3 ? 4 : kWordsPerExt;
    static_assert(kUnitsPerElem > 0 && kWordsPerExt > 0);
    static_assert(kUnitsPerElem * WordsPerUnit_V + kWordsPerExt == WordsPerElem_V);
};

template<typename>
struct SOABufferInitData;
template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
struct SOABufferInitData<SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>>
{
    using Trait = SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>;

    std::array<std::vector<uint32_t>, Trait::kUnitsPerElem> unitData;
    std::vector<uint32_t> extData;

    SOABufferInitData() = default;

    template<std::ranges::input_range ElementRange_T>
    SOABufferInitData(ElementRange_T&& elements, uint elementCount)
    {
        static_assert(sizeof(std::ranges::range_value_t<ElementRange_T>) == sizeof(Word_T) * WordsPerElem_V);

        for (uint32_t unitIdx = 0; unitIdx < Trait::kUnitsPerElem; ++unitIdx)
            unitData[unitIdx].resize(elementCount * WordsPerUnit_V);
        extData.resize(elementCount * Trait::kPaddedWordsPerExt);

        for (uint32_t elemIdx = 0; const auto& element : elements)
        {
            if (elemIdx >= elementCount)
                break;
            auto pElementWords = reinterpret_cast<const uint32_t*>(&element);

            for (uint32_t unitIdx = 0; unitIdx < Trait::kUnitsPerElem; ++unitIdx)
            {
                std::copy(pElementWords, pElementWords + WordsPerUnit_V, unitData[unitIdx].data() + elemIdx * WordsPerUnit_V);
                pElementWords += WordsPerUnit_V;
            }
            std::copy(pElementWords, pElementWords + Trait::kWordsPerExt, extData.data() + elemIdx * Trait::kPaddedWordsPerExt);

            ++elemIdx;
        }
    }

    bool isEmpty() const
    {
        return std::ranges::all_of(unitData, [](const auto& data) { return data.empty(); }) && extData.empty();
    }

    bool isCapable(uint32_t elementCount) const
    {
        if (unitData.size() != Trait::kUnitsPerElem)
            return false;
        for (const auto& data : unitData)
            if (elementCount * WordsPerUnit_V > data.size())
                return false;
        if (elementCount * Trait::kPaddedWordsPerExt > extData.size())
            return false;
        return true;
    }
};

template<typename>
struct SOABuffer;
template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
struct SOABuffer<SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>>
{
    using Trait = SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>;

    std::array<ref<Buffer>, Trait::kUnitsPerElem> pUnitBuffers;
    ref<Buffer> pExtBuffer;

    SOABuffer() = default;
    SOABuffer(const ref<Device>& pDevice, uint elementCount, ResourceBindFlags bindFlags, const SOABufferInitData<Trait>& initData = {})
    {
        bool isInitDataCapable = initData.isCapable(elementCount);
        for (uint32_t unitIdx = 0; unitIdx < Trait::kUnitsPerElem; ++unitIdx)
        {
            const uint32_t* pInitData = isInitDataCapable ? initData.unitData[unitIdx].data() : nullptr;
            pUnitBuffers[unitIdx] = pDevice->createStructuredBuffer(
                WordsPerUnit_V * sizeof(Word_T), elementCount, bindFlags, MemoryType::DeviceLocal, pInitData, false
            );
        }
        {
            const uint32_t* pInitData = isInitDataCapable ? initData.extData.data() : nullptr;
            pExtBuffer = pDevice->createStructuredBuffer(
                Trait::kPaddedWordsPerExt * sizeof(Word_T), elementCount, bindFlags, MemoryType::DeviceLocal, pInitData, false
            );
        }
    }

    bool isCapable(uint32_t elementCount) const
    {
        for (uint32_t unitIdx = 0; unitIdx < Trait::kUnitsPerElem; ++unitIdx)
        {
            const auto& pUnitBuffer = pUnitBuffers[unitIdx];
            if (pUnitBuffer == nullptr || pUnitBuffer->getStructSize() != WordsPerUnit_V * sizeof(Word_T) ||
                pUnitBuffer->getElementCount() < elementCount)
                return false;
        }
        if (pExtBuffer == nullptr || pExtBuffer->getStructSize() != Trait::kPaddedWordsPerExt * sizeof(Word_T) ||
            pExtBuffer->getElementCount() < elementCount)
            return false;
        return true;
    }

    void bindShaderData(const ShaderVar& var) const
    {
        for (uint32_t unitIdx = 0; unitIdx < Trait::kUnitsPerElem; ++unitIdx)
            var["unitBufs"][unitIdx] = pUnitBuffers[unitIdx];
        var["extBuf"] = pExtBuffer;
    }

    void clearUAV(RenderContext* pRenderContext) const
    {
        for (const auto& pUnitBuffer : pUnitBuffers)
            pRenderContext->clearUAV(pUnitBuffer->getUAV().get(), uint4{});
        pRenderContext->clearUAV(pExtBuffer->getUAV().get(), uint4{});
    }
};

} // namespace GSGI

#endif // GSGI_SOABUFFERUTIL_HPP
