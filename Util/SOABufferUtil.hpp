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
    static constexpr uint32_t kUnitsPerElem =
        (WordsPerElem_V % WordsPerUnit_V == 0) ? (WordsPerElem_V / WordsPerUnit_V - 1) : (WordsPerElem_V / WordsPerUnit_V);
    static constexpr uint32_t kWordsPerExt = (WordsPerElem_V % WordsPerUnit_V == 0) ? WordsPerUnit_V : (WordsPerElem_V % WordsPerUnit_V);
    static constexpr uint32_t kPaddedWordsPerExt = kWordsPerExt == 3 ? 4 : kWordsPerExt;
    static_assert(kUnitsPerElem > 0 && kWordsPerExt > 0);
    static_assert(kUnitsPerElem * WordsPerUnit_V + kWordsPerExt == WordsPerElem_V);
};

struct SOABufferInitData
{
    std::vector<std::vector<uint32_t>> unitData;
    std::vector<uint32_t> extData;

    template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V, std::ranges::input_range ElementRange_T>
    static SOABufferInitData create(
        SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V> trait,
        ElementRange_T&& elements,
        uint elementCount
    )
    {
        static_assert(sizeof(std::ranges::range_value_t<ElementRange_T>) == sizeof(Word_T) * WordsPerElem_V);
        SOABufferInitData soaInitData;
        soaInitData.unitData.resize(trait.kUnitsPerElem);
        for (uint32_t unitIdx = 0; unitIdx < trait.kUnitsPerElem; ++unitIdx)
            soaInitData.unitData[unitIdx].resize(elementCount * WordsPerUnit_V);
        soaInitData.extData.resize(elementCount * trait.kPaddedWordsPerExt);

        for (uint32_t elemIdx = 0; const auto& element : elements)
        {
            if (elemIdx >= elementCount)
                break;
            auto pElementWords = reinterpret_cast<const uint32_t*>(&element);

            for (uint32_t unitIdx = 0; unitIdx < trait.kUnitsPerElem; ++unitIdx)
            {
                std::copy(pElementWords, pElementWords + WordsPerUnit_V, soaInitData.unitData[unitIdx].data() + elemIdx * WordsPerUnit_V);
                pElementWords += WordsPerUnit_V;
            }
            std::copy(pElementWords, pElementWords + trait.kWordsPerExt, soaInitData.extData.data() + elemIdx * trait.kPaddedWordsPerExt);

            ++elemIdx;
        }
        return soaInitData;
    }

    bool isEmpty() const { return unitData.empty() && extData.empty(); }

    template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
    bool isCapable(SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V> trait, uint32_t elementCount) const
    {
        if (unitData.size() != trait.kUnitsPerElem)
            return false;
        for (const auto& data : unitData)
            if (elementCount * WordsPerUnit_V > data.size())
                return false;
        if (elementCount * trait.kPaddedWordsPerExt > extData.size())
            return false;
        return true;
    }
};

struct SOABuffer
{
    std::vector<ref<Buffer>> pUnitBuffers;
    ref<Buffer> pExtBuffer;

    template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
    static SOABuffer create(
        SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V> trait,
        const ref<Device>& pDevice,
        uint elementCount,
        ResourceBindFlags bindFlags,
        const SOABufferInitData& initData = {}
    )
    {
        SOABuffer soa;
        soa.pUnitBuffers.resize(trait.kUnitsPerElem);
        bool isInitDataCapable = initData.isCapable(trait, elementCount);
        for (uint32_t unitIdx = 0; unitIdx < trait.kUnitsPerElem; ++unitIdx)
        {
            const uint32_t* pInitData = isInitDataCapable ? initData.unitData[unitIdx].data() : nullptr;
            soa.pUnitBuffers[unitIdx] = pDevice->createStructuredBuffer(
                WordsPerUnit_V * sizeof(Word_T), elementCount, bindFlags, MemoryType::DeviceLocal, pInitData, false
            );
        }
        {
            const uint32_t* pInitData = isInitDataCapable ? initData.extData.data() : nullptr;
            soa.pExtBuffer = pDevice->createStructuredBuffer(
                trait.kPaddedWordsPerExt * sizeof(Word_T), elementCount, bindFlags, MemoryType::DeviceLocal, pInitData, false
            );
        }
        return soa;
    }

    template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
    static bool isCapable(
        SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V> trait,
        uint32_t elementCount,
        const auto&... soaBuffers
    )
    {
        return (soaBuffers.isCapable(trait, elementCount) && ...);
    }

    template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
    bool isCapable(SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V> trait, uint32_t elementCount)
    {
        if (pUnitBuffers.size() != trait.kUnitsPerElem)
            return false;
        for (uint32_t unitIdx = 0; unitIdx < trait.kUnitsPerElem; ++unitIdx)
        {
            const auto& pUnitBuffer = pUnitBuffers[unitIdx];
            if (pUnitBuffer == nullptr || pUnitBuffer->getStructSize() != WordsPerUnit_V * sizeof(Word_T) ||
                pUnitBuffer->getElementCount() < elementCount)
                return false;
        }
        if (pExtBuffer == nullptr || pExtBuffer->getStructSize() != trait.kPaddedWordsPerExt * sizeof(Word_T) ||
            pExtBuffer->getElementCount() < elementCount)
            return false;
        return true;
    }

    void bindShaderData(const ShaderVar& var)
    {
        for (uint32_t unitIdx = 0; unitIdx < pUnitBuffers.size(); ++unitIdx)
            var["unitBufs"][unitIdx] = pUnitBuffers[unitIdx];
        var["extBuf"] = pExtBuffer;
    }

    void clearUAV(RenderContext* pRenderContext)
    {
        for (const auto& pUnitBuffer : pUnitBuffers)
            pRenderContext->clearUAV(pUnitBuffer->getUAV().get(), uint4{});
        pRenderContext->clearUAV(pExtBuffer->getUAV().get(), uint4{});
    }
};

} // namespace GSGI

#endif // GSGI_SOABUFFERUTIL_HPP
