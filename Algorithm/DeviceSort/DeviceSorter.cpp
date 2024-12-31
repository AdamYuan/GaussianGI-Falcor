//
// Created by adamyuan on 12/19/24.
//
#include "DeviceSorter.hpp"

#include "../../Util/DeviceUtil.hpp"
#include "DeviceSorterSizes.slangh"

namespace GSGI
{

namespace
{
inline constexpr uint kBitsPerPass = BITS_PER_PASS; // 8
inline constexpr uint kRadix = RADIX;               // 1 << 8
inline constexpr uint kRadixPasses = 4;
inline constexpr uint kPartitionSize = SORT_PART_SIZE;
inline constexpr uint kGlobalHistPartSize = GLOBAL_HIST_PART_SIZE;
} // namespace

DeviceSortDesc::DeviceSortDesc(const std::vector<DeviceSortBufferType>& bufferTypes) : mBufferCount(bufferTypes.size())
{
    std::vector<bool> isActiveBufferID(mBufferCount, true);
    const auto getActiveBufferIDs = [&](uint32_t excludeBufferID)
    {
        std::vector<uint32_t> bufferIDs;
        for (uint32_t bufferID = 0; bufferID < bufferTypes.size(); ++bufferID)
            if (isActiveBufferID[bufferID] && bufferID != excludeBufferID)
                bufferIDs.push_back(bufferID);
        return bufferIDs;
    };

    for (uint32_t bufferID = 0; bufferID < bufferTypes.size(); ++bufferID)
    {
        DeviceSortBufferType bufferType = bufferTypes[bufferID];
        bool isPayload = static_cast<uint32_t>(bufferType) & 1u;
        uint32_t keyBitWidth = static_cast<uint32_t>(bufferType) & (~1u);
        uint32_t keyPassCount = keyBitWidth / kBitsPerPass;

        auto payloadBufferIDs = getActiveBufferIDs(bufferID);
        for (uint32_t keyPassID = 0; keyPassID < keyPassCount; ++keyPassID)
        {
            mPassDescs.push_back({
                .keyBufferID = bufferID,
                .keyRadixShift = keyPassID * kBitsPerPass,
                .keyWrite = isPayload || keyPassID < keyPassCount - 1,
                .payloadBufferIDs = payloadBufferIDs,
            });
        }

        if (!isPayload)
            isActiveBufferID[bufferID] = false;
    }
}

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
DeviceSorterResource<Type_V, DispatchType_V> DeviceSorterResource<Type_V, DispatchType_V>::create(const ref<Device>& pDevice, uint maxCount)
{
    static_assert(std::same_as<uint, uint32_t>);

    DeviceSorterResource res = {.maxCount = maxCount};
    res.pTempKeyBuffer = pDevice->createStructuredBuffer(sizeof(uint), maxCount);
    res.pGlobalHistBuffer = pDevice->createStructuredBuffer(sizeof(uint), kRadix * kRadixPasses);
    res.pPassHistBuffer = pDevice->createStructuredBuffer(sizeof(uint), kRadix * kRadixPasses * div_round_up(maxCount, kPartitionSize));
    res.pIndexBuffer = pDevice->createStructuredBuffer(sizeof(uint), kRadixPasses);
    if constexpr (Type_V == DeviceSortType::kPair)
        res.pTempPayloadBuffer = pDevice->createStructuredBuffer(sizeof(uint), maxCount);
    if constexpr (DispatchType_V == DeviceSortDispatchType::kIndirect)
    {
        DispatchArguments args[] = {{0, 1, 1}, {0, 1, 1}};
        static_assert(sizeof(DispatchArguments) == sizeof(uint32_t) * 3);
        res.pIndirectBuffer = pDevice->createStructuredBuffer(
            sizeof(DispatchArguments),
            2,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::IndirectArg,
            MemoryType::DeviceLocal,
            args
        );
    }
    return res;
}

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
bool DeviceSorterResource<Type_V, DispatchType_V>::isCapable(uint maxCount) const
{
    if (maxCount > this->maxCount)
        return false;
    bool isBufferCapable = pTempKeyBuffer && pGlobalHistBuffer && pPassHistBuffer && pIndexBuffer;
    if constexpr (Type_V == DeviceSortType::kPair)
        isBufferCapable = isBufferCapable && pTempPayloadBuffer;
    if constexpr (DispatchType_V == DeviceSortDispatchType::kIndirect)
        isBufferCapable = isBufferCapable && pIndirectBuffer;
    return isBufferCapable;
}

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
void DeviceSorter<Type_V, DispatchType_V>::bindCount(
    const ShaderVar& var,
    uint count,
    const ref<Buffer>& pCountBuffer,
    uint64_t countBufferOffset
)
{
    if constexpr (DispatchType_V == DeviceSortDispatchType::kIndirect)
    {
        FALCOR_CHECK(countBufferOffset % sizeof(uint32_t) == 0, "countBufferOffset must be aligned to 4");
        uint32_t index = countBufferOffset / sizeof(uint32_t);
        FALCOR_CHECK(uint64_t(index) * sizeof(uint32_t) == countBufferOffset, "countBufferOffset too large");
        var["gKeyCountBuffer"] = pCountBuffer;
        var["gKeyCountBufferIndex"] = index;
    }
    else
        var["gKeyCount"] = count;
}

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
DeviceSorter<Type_V, DispatchType_V>::DeviceSorter(const ref<Device>& pDevice)
{
    DefineList defList{};
    defList.add("SORT_PAIRS", Type_V == DeviceSortType::kPair ? "1" : "0");
    defList.add("SORT_INDIRECT", DispatchType_V == DeviceSortDispatchType::kIndirect ? "1" : "0");
    defList.add("LANES_PER_WAVE", fmt::to_string(deviceWaveGetLaneCount(pDevice)));
    mpResetPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/DeviceSort/Reset.cs.slang", "csMain", defList);
    mpGlobalHistPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/DeviceSort/GlobalHist.cs.slang", "csMain", defList);
    mpScanHistPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/DeviceSort/ScanHist.cs.slang", "csMain", defList);
    mpOneSweepPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/DeviceSort/OneSweep.cs.slang", "csMain", defList);
}

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
void DeviceSorter<Type_V, DispatchType_V>::dispatchImpl(
    ComputeContext* pComputeContext,
    const ref<Buffer>& pKeyBuffer,
    const ref<Buffer>& pPayloadBuffer,
    uint count,
    const ref<Buffer>& pCountBuffer,
    uint64_t countBufferOffset,
    const DeviceSorterResource<Type_V, DispatchType_V>& resource
)
{
    if constexpr (DispatchType_V == DeviceSortDispatchType::kDirect)
    {
        if (count == 0)
            return;
    }
    FALCOR_CHECK(resource.isCapable(count), "DeviceSorterResource not capable");
    /* FALCOR_CHECK(pKeyBuffer->getSize() == resource.pTempKeyBuffer->getSize(), "Key buffer size not match");
    if constexpr (Type_V == DeviceSortType::kPair)
        FALCOR_CHECK(pPayloadBuffer->getSize() == resource.pTempPayloadBuffer->getSize(), "Payload buffer size not match"); */

    {
        const auto& pPass = mpResetPass;
        auto var = pPass->getRootVar();
        bindCount(var, count, pCountBuffer, countBufferOffset);
        var["gPassHists"] = resource.pPassHistBuffer;
        var["gGlobalHists"] = resource.pGlobalHistBuffer;
        var["gIndices"] = resource.pIndexBuffer;
        if constexpr (DispatchType_V == DeviceSortDispatchType::kIndirect)
            var["gIndirectArgs"] = resource.pIndirectBuffer;
        pPass->execute(pComputeContext, 256 * pPass->getThreadGroupSize().x, 1, 1);
    }

    {
        const auto& pPass = mpGlobalHistPass;
        auto var = pPass->getRootVar();
        bindCount(var, count, pCountBuffer, countBufferOffset);
        var["gSortKeys"] = pKeyBuffer;
        var["gGlobalHists"] = resource.pGlobalHistBuffer;
        /* bindCount(var, count, pCountBuffer, countBufferOffset);
        var["b_sort"] = pKeyBuffer;
        var["b_globalHist"] = resource.pGlobalHistBuffer; */
        if constexpr (DispatchType_V == DeviceSortDispatchType::kDirect)
            pPass->execute(pComputeContext, div_round_up(count, kGlobalHistPartSize) * pPass->getThreadGroupSize().x, 1, 1);
        else
            pPass->executeIndirect(pComputeContext, resource.pIndirectBuffer.get(), 0 * sizeof(DispatchArguments));
    }

    {
        const auto& pPass = mpScanHistPass;
        auto var = pPass->getRootVar();
        bindCount(var, count, pCountBuffer, countBufferOffset);
        var["gPassHists"] = resource.pPassHistBuffer;
        var["gGlobalHists"] = resource.pGlobalHistBuffer;
        pPass->execute(pComputeContext, kRadixPasses * pPass->getThreadGroupSize().x, 1, 1);
    }

    {
        const auto& pPass = mpOneSweepPass;
        auto var = pPass->getRootVar();
        bindCount(var, count, pCountBuffer, countBufferOffset);
        var["gPassHists"] = resource.pPassHistBuffer;
        var["gIndices"] = resource.pIndexBuffer;

        bool flip = false;
        for (uint radixShift = 0; radixShift < 32; radixShift += 8, flip = !flip)
        {
            var["gRadixShift"] = radixShift;
            var["gPassIdx"] = radixShift / 8;
            var["gSortKeys"] = flip ? resource.pTempKeyBuffer : pKeyBuffer;
            var["gAltKeys"] = flip ? pKeyBuffer : resource.pTempKeyBuffer;

            if constexpr (Type_V == DeviceSortType::kPair)
            {
                var["gSortPayloads"] = flip ? resource.pTempPayloadBuffer : pPayloadBuffer;
                var["gAltPayloads"] = flip ? pPayloadBuffer : resource.pTempPayloadBuffer;
            }

            if constexpr (DispatchType_V == DeviceSortDispatchType::kDirect)
                pPass->execute(pComputeContext, div_round_up(count, kPartitionSize) * pPass->getThreadGroupSize().x, 1, 1);
            else
                pPass->executeIndirect(pComputeContext, resource.pIndirectBuffer.get(), 1 * sizeof(DispatchArguments));
        }
    }
}

template struct DeviceSorterResource<DeviceSortType::kKey, DeviceSortDispatchType::kDirect>;
template struct DeviceSorterResource<DeviceSortType::kPair, DeviceSortDispatchType::kDirect>;
template struct DeviceSorterResource<DeviceSortType::kKey, DeviceSortDispatchType::kIndirect>;
template struct DeviceSorterResource<DeviceSortType::kPair, DeviceSortDispatchType::kIndirect>;

template class DeviceSorter<DeviceSortType::kKey, DeviceSortDispatchType::kDirect>;
template class DeviceSorter<DeviceSortType::kPair, DeviceSortDispatchType::kDirect>;
template class DeviceSorter<DeviceSortType::kKey, DeviceSortDispatchType::kIndirect>;
template class DeviceSorter<DeviceSortType::kPair, DeviceSortDispatchType::kIndirect>;

} // namespace GSGI
