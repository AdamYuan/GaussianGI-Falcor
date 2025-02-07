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
struct SOABufferData;
template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
struct SOABufferData<SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>>
{
    using Trait = SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>;

    std::array<std::vector<uint32_t>, Trait::kUnitsPerElem> unitData;
    std::vector<uint32_t> extData;

    SOABufferData() = default;

    template<std::ranges::input_range ElementRange_T>
    SOABufferData(ElementRange_T&& elements, uint elementCount)
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
                std::copy_n(pElementWords, WordsPerUnit_V, unitData[unitIdx].data() + elemIdx * WordsPerUnit_V);
                pElementWords += WordsPerUnit_V;
            }
            std::copy_n(pElementWords, Trait::kWordsPerExt, extData.data() + elemIdx * Trait::kPaddedWordsPerExt);

            ++elemIdx;
        }
    }

    template<typename Element_T>
    Element_T getElement(uint elemIdx) const
        requires(sizeof(Element_T) == sizeof(Word_T) * WordsPerElem_V)
    {
        Element_T element;
        auto pElementWords = reinterpret_cast<uint32_t*>(&element);
        for (uint32_t unitIdx = 0; unitIdx < Trait::kUnitsPerElem; ++unitIdx)
        {
            std::copy_n(unitData[unitIdx].data() + elemIdx * WordsPerUnit_V, WordsPerUnit_V, pElementWords);
            pElementWords += WordsPerUnit_V;
        }
        std::copy_n(extData.data() + elemIdx * Trait::kPaddedWordsPerExt, Trait::kWordsPerExt, pElementWords);
        return element;
    }

    template<typename Element_T>
    std::vector<Element_T> getElements(uint32_t elementCount) const
    {
        FALCOR_CHECK(isCapable(elementCount), "Unable to get elements of elementCount");
        std::vector<Element_T> elements(elementCount);
        for (uint elemIdx = 0; elemIdx < elementCount; ++elemIdx)
            elements[elemIdx] = getElement<Element_T>(elemIdx);
        return elements;
    }

    bool isEmpty() const
    {
        return std::ranges::all_of(unitData, [](const auto& data) { return data.empty(); }) && extData.empty();
    }

    bool isCapable(uint32_t elementCount) const
    {
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
    SOABuffer(const ref<Device>& pDevice, uint elementCount, ResourceBindFlags bindFlags, const SOABufferData<Trait>& initData = {})
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

    SOABufferData<Trait> getData(uint firstElement = 0, uint elementCount = 0) const
    {
        SOABufferData<Trait> data = {};
        for (uint32_t unitIdx = 0; unitIdx < Trait::kUnitsPerElem; ++unitIdx)
            data.unitData[unitIdx] =
                pUnitBuffers[unitIdx]->template getElements<uint32_t>(firstElement * WordsPerUnit_V, elementCount * WordsPerUnit_V);
        data.extData =
            pExtBuffer->getElements<uint32_t>(firstElement * Trait::kPaddedWordsPerExt, elementCount * Trait::kPaddedWordsPerExt);
        return data;
    }
};

} // namespace GSGI

#endif // GSGI_SOABUFFERUTIL_HPP
