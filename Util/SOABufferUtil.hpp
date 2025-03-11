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
    static_assert(WordsPerElem_V > 0);
    static constexpr uint32_t kWordsPerElem = WordsPerElem_V;
    static constexpr uint32_t kUnitsPerElem = WordsPerElem_V / WordsPerUnit_V;
    static constexpr uint32_t kWordsPerExt = WordsPerElem_V % WordsPerUnit_V;
    static constexpr uint32_t kPaddedWordsPerExt = kWordsPerExt == 3 ? 4 : kWordsPerExt;
    static constexpr bool kHasUnits = kUnitsPerElem > 0;
    static constexpr bool kHasExt = kWordsPerExt > 0;
    static_assert(kHasUnits || kHasExt);
    static constexpr uint32_t kVectorsPerElem = kUnitsPerElem + kHasExt;
    static constexpr bool isExtVector(uint32_t vecID) { return kHasExt && vecID == kUnitsPerElem; }
    static constexpr uint32_t getWordsPerVector(uint32_t vecID) { return isExtVector(vecID) ? kWordsPerExt : WordsPerUnit_V; }
    static constexpr uint32_t getPaddedWordsPerVector(uint32_t vecID) { return isExtVector(vecID) ? kPaddedWordsPerExt : WordsPerUnit_V; }
    static constexpr ResourceFormat getPaddedVectorFormat(uint32_t vecID)
    {
        uint32_t count = getPaddedWordsPerVector(vecID);

#define GSGI_SAO_X(C_TYPE, R_TYPE)                 \
    if constexpr (std::same_as<Word_T, C_TYPE>)    \
    {                                              \
        if (count == 1)                            \
            return ResourceFormat::R32##R_TYPE;    \
        if (count == 2)                            \
            return ResourceFormat::RG32##R_TYPE;   \
        if (count == 3)                            \
            return ResourceFormat::RGB32##R_TYPE;  \
        if (count == 4)                            \
            return ResourceFormat::RGBA32##R_TYPE; \
    }
        GSGI_SAO_X(float, Float)
        else GSGI_SAO_X(uint32_t, Uint)   //
            else GSGI_SAO_X(int32_t, Int) //
            else static_assert(false);
        return ResourceFormat::Unknown;
#undef GSGI_SAO_X
    }
    static_assert(kUnitsPerElem * WordsPerUnit_V + kWordsPerExt == WordsPerElem_V);
};

template<typename>
struct SOABufferData;
template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V>
struct SOABufferData<SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>>
{
    using Trait = SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>;

    std::array<std::vector<uint32_t>, Trait::kVectorsPerElem> vectorData;

    SOABufferData() = default;

    template<std::ranges::input_range ElementRange_T>
    SOABufferData(ElementRange_T&& elements, uint elementCount)
    {
        static_assert(sizeof(std::ranges::range_value_t<ElementRange_T>) == sizeof(Word_T) * WordsPerElem_V);

        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
            vectorData[vecIdx].resize(elementCount * Trait::getPaddedWordsPerVector(vecIdx));

        for (uint32_t elemIdx = 0; const auto& element : elements)
        {
            if (elemIdx >= elementCount)
                break;
            auto pElementWords = reinterpret_cast<const uint32_t*>(&element);

            for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
            {
                std::copy_n(
                    pElementWords,
                    Trait::getWordsPerVector(vecIdx),
                    vectorData[vecIdx].data() + elemIdx * Trait::getPaddedWordsPerVector(vecIdx)
                );
                pElementWords += Trait::getWordsPerVector(vecIdx);
            }

            ++elemIdx;
        }
    }

    template<typename Element_T>
    Element_T getElement(uint elemIdx) const
        requires(sizeof(Element_T) == sizeof(Word_T) * WordsPerElem_V)
    {
        Element_T element;
        auto pElementWords = reinterpret_cast<uint32_t*>(&element);
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            std::copy_n(
                vectorData[vecIdx].data() + elemIdx * Trait::getPaddedWordsPerVector(vecIdx),
                Trait::getWordsPerVector(vecIdx),
                pElementWords
            );
            pElementWords += Trait::getWordsPerVector(vecIdx);
        }
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
        return std::ranges::all_of(vectorData, [](const auto& data) { return data.empty(); });
    }

    bool isCapable(uint32_t elementCount) const
    {
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
            if (elementCount * Trait::getPaddedWordsPerVector(vecIdx) > vectorData[vecIdx].size())
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

    std::array<ref<Buffer>, Trait::kVectorsPerElem> pVectorBuffers;

    SOABuffer() = default;
    SOABuffer(const ref<Device>& pDevice, uint elementCount, ResourceBindFlags bindFlags, const SOABufferData<Trait>& initData = {})
    {
        bool isInitDataCapable = initData.isCapable(elementCount);
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            const uint32_t* pInitData = isInitDataCapable ? initData.vectorData[vecIdx].data() : nullptr;
            pVectorBuffers[vecIdx] = pDevice->createStructuredBuffer(
                Trait::getPaddedWordsPerVector(vecIdx) * sizeof(Word_T), elementCount, bindFlags, MemoryType::DeviceLocal, pInitData, false
            );
        }
    }

    bool isCapable(uint32_t elementCount) const
    {
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            const auto& pBuffer = pVectorBuffers[vecIdx];
            if (pBuffer == nullptr || pBuffer->getStructSize() != Trait::getPaddedWordsPerVector(vecIdx) * sizeof(Word_T) ||
                pBuffer->getElementCount() < elementCount)
                return false;
        }
        return true;
    }

    void bindShaderData(const ShaderVar& var) const
    {
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            if (Trait::isExtVector(vecIdx))
                var["extBuf"] = pVectorBuffers[vecIdx];
            else
                var["unitBufs"][vecIdx] = pVectorBuffers[vecIdx];
        }
    }

    void clearUAV(RenderContext* pRenderContext) const
    {
        for (const auto& pBuffer : pVectorBuffers)
            pRenderContext->clearUAV(pBuffer->getUAV().get(), uint4{});
    }

    SOABufferData<Trait> getData(uint firstElement = 0, uint elementCount = 0) const
    {
        SOABufferData<Trait> data = {};
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            const auto& pBuffer = pVectorBuffers[vecIdx];
            data.vectorData[vecIdx] = pBuffer->template getElements<uint32_t>(
                firstElement * Trait::getPaddedWordsPerVector(vecIdx), elementCount * Trait::getPaddedWordsPerVector(vecIdx)
            );
        }
        return data;
    }
};

template<typename, uint32_t>
struct SOATexture;
template<typename Word_T, uint32_t WordsPerUnit_V, uint32_t WordsPerElem_V, uint32_t Dim_V>
struct SOATexture<SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>, Dim_V>
{
    static_assert(Dim_V == 1 || Dim_V == 2 || Dim_V == 3);

    using Trait = SOATrait<SOAUnitTrait<Word_T, WordsPerUnit_V>, WordsPerElem_V>;

    std::array<ref<Texture>, Trait::kVectorsPerElem> pVectorTextures;

    SOATexture() = default;
    SOATexture(const ref<Device>& pDevice, math::vector<uint32_t, Dim_V> resolution, ResourceBindFlags bindFlags)
    {
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            if constexpr (Dim_V == 1)
                pVectorTextures[vecIdx] =
                    pDevice->createTexture1D(resolution.x, Trait::getPaddedVectorFormat(vecIdx), 1, 1, nullptr, bindFlags);
            else if constexpr (Dim_V == 2)
                pVectorTextures[vecIdx] =
                    pDevice->createTexture2D(resolution.x, resolution.y, Trait::getPaddedVectorFormat(vecIdx), 1, 1, nullptr, bindFlags);
            else if constexpr (Dim_V == 3)
                pVectorTextures[vecIdx] = pDevice->createTexture3D(
                    resolution.x, resolution.y, resolution.z, Trait::getPaddedVectorFormat(vecIdx), 1, nullptr, bindFlags
                );
        }
    }

    bool isCapable(math::vector<uint32_t, Dim_V> resolution) const
    {
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            const auto& pTexture = pVectorTextures[vecIdx];
            if (pTexture == nullptr || pTexture->getFormat() != Trait::getPaddedVectorFormat(vecIdx))
                return false;
            if constexpr (Dim_V >= 1)
                if (pTexture->getWidth() != resolution.x)
                    return false;
            if constexpr (Dim_V >= 2)
                if (pTexture->getHeight() != resolution.y)
                    return false;
            if constexpr (Dim_V >= 3)
                if (pTexture->getDepth() != resolution.z)
                    return false;
        }
        return true;
    }

    void bindShaderData(const ShaderVar& var) const
    {
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            if (Trait::isExtVector(vecIdx))
                var["extTex"] = pVectorTextures[vecIdx];
            else
                var["unitTexs"][vecIdx] = pVectorTextures[vecIdx];
        }
    }

    template<typename Element_T>
    void clearTexture(RenderContext* pRenderContext, const Element_T& value) const
        requires(sizeof(Element_T) == sizeof(Word_T) * WordsPerElem_V)
    {
        auto pElementWords = reinterpret_cast<const float*>(&value);
        for (uint32_t vecIdx = 0; vecIdx < Trait::kVectorsPerElem; ++vecIdx)
        {
            float4 clearValue{};
            std::copy_n(pElementWords, Trait::getWordsPerVector(vecIdx), &clearValue[0]);
            pRenderContext->clearTexture(pVectorTextures[vecIdx].get(), clearValue);

            pElementWords += Trait::getWordsPerVector(vecIdx);
        }
    }

    void clearTexture(RenderContext* pRenderContext) const
    {
        for (const auto& pTexture : pVectorTextures)
            pRenderContext->clearTexture(pTexture.get(), float4{});
    }
};

} // namespace GSGI

#endif // GSGI_SOABUFFERUTIL_HPP
