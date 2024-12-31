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
inline constexpr uint kPartitionSize = SORT_PART_SIZE;
inline constexpr uint kGlobalHistPartSize = GLOBAL_HIST_PART_SIZE;
} // namespace

DeviceSortDesc::DeviceSortDesc(const std::vector<DeviceSortBufferType>& bufferTypes) : mBufferCount(bufferTypes.size())
{
    FALCOR_CHECK(!bufferTypes.empty(), "");
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

        bool isKey = keyBitWidth;
        if (isKey)
        {
            uint32_t keyPassCount = keyBitWidth / kBitsPerPass;
            uint32_t keyID = mKeyBufferDescs.size();
            mKeyBufferDescs.push_back({
                .bufferID = bufferID,
                .bitWidth = keyBitWidth,
                .isPayload = isPayload,
                .payloadBufferIDs = getActiveBufferIDs(bufferID),
                .firstPassID = (uint32_t)mPassDescs.size(),
                .passCount = keyPassCount,
            });

            for (uint32_t keyPassID = 0; keyPassID < keyPassCount; ++keyPassID)
            {
                mPassDescs.push_back({
                    .keyID = keyID,
                    .keyRadixShift = keyPassID * kBitsPerPass,
                    .keyWrite = isPayload || keyPassID < keyPassCount - 1,
                });
            }
        }

        if (!isPayload)
            isActiveBufferID[bufferID] = false;
    }
}

template<DeviceSortDispatchType DispatchType_V>
DeviceSortResource<DispatchType_V> DeviceSortResource<DispatchType_V>::create(
    const ref<Device>& pDevice,
    uint maxBufferCount,
    uint maxPassCount,
    uint maxKeyCount
)
{
    static_assert(std::same_as<uint, uint32_t>);

    DeviceSortResource res = {
        .maxPassCount = maxPassCount,
        .maxKeyCount = maxKeyCount,
    };
    for (uint32_t i = 0; i < maxBufferCount; ++i)
        res.pTempBuffers.push_back(pDevice->createStructuredBuffer(sizeof(uint), maxKeyCount));
    res.pGlobalHistBuffer = pDevice->createStructuredBuffer(sizeof(uint), kRadix * maxPassCount);
    res.pPassHistBuffer = pDevice->createStructuredBuffer(sizeof(uint), kRadix * maxPassCount * div_round_up(maxKeyCount, kPartitionSize));
    res.pIndexBuffer = pDevice->createStructuredBuffer(sizeof(uint), maxPassCount);
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

template<DeviceSortDispatchType DispatchType_V>
bool DeviceSortResource<DispatchType_V>::isCapable(uint bufferCount, uint passCount, uint keyCount) const
{
    if (keyCount > this->maxKeyCount || passCount > this->maxPassCount || bufferCount > this->pTempBuffers.size())
        return false;
    bool isBufferCapable = std::all_of(pTempBuffers.begin(), pTempBuffers.begin() + bufferCount, [](const auto& x) { return bool(x); }) &&
                           pGlobalHistBuffer && pPassHistBuffer && pIndexBuffer;
    if constexpr (DispatchType_V == DeviceSortDispatchType::kIndirect)
        isBufferCapable = isBufferCapable && pIndirectBuffer;
    return isBufferCapable;
}
template<DeviceSortDispatchType DispatchType_V>
void DeviceSorter<DispatchType_V>::bindCount(const ShaderVar& var, uint count, const ref<Buffer>& pCountBuffer, uint64_t countBufferOffset)
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

template<DeviceSortDispatchType DispatchType_V>
DeviceSorter<DispatchType_V>::DeviceSorter(const ref<Device>& pDevice, DeviceSortDesc desc) : mDesc{std::move(desc)}
{
    DefineList defList{};
    // defList.add("SORT_PAIRS", Type_V == DeviceSortType::kPair ? "1" : "0");
    defList.add("PASS_COUNT", fmt::to_string(mDesc.getPassCount()));
    defList.add("PAYLOAD_BUFFER_COUNT", fmt::to_string(mDesc.getPassMaxPayloadBufferCount()));
    defList.add("KEY_BUFFER_COUNT", fmt::to_string(mDesc.getKeyBufferCount()));
    defList.add("SORT_INDIRECT", DispatchType_V == DeviceSortDispatchType::kIndirect ? "1" : "0");
    defList.add("LANES_PER_WAVE", fmt::to_string(deviceWaveGetLaneCount(pDevice)));
    mpResetPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/DeviceSort/Reset.cs.slang", "csMain", defList);
    mpGlobalHistPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/DeviceSort/GlobalHist.cs.slang", "csMain", defList);
    mpScanHistPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/DeviceSort/ScanHist.cs.slang", "csMain", defList);
    mpOneSweepPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/DeviceSort/OneSweep.cs.slang", "csMain", defList);
}

template<DeviceSortDispatchType DispatchType_V>
void DeviceSorter<DispatchType_V>::dispatchImpl(
    ComputeContext* pComputeContext,
    const std::vector<ref<Buffer>>& pBuffers,
    uint count,
    const ref<Buffer>& pCountBuffer,
    uint64_t countBufferOffset,
    const DeviceSortResource<DispatchType_V>& resource
)
{
    if constexpr (DispatchType_V == DeviceSortDispatchType::kDirect)
    {
        if (count == 0)
            return;
    }
    FALCOR_CHECK(resource.isCapable(mDesc, count), "DeviceSortResource not capable");
    FALCOR_CHECK(pBuffers.size() == mDesc.getBufferCount(), "bufferCount not matched");

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
        for (uint32_t keyID = 0; keyID < mDesc.getKeyBufferCount(); ++keyID)
            var["gSrcKeys"][keyID] = pBuffers[mDesc.getKeyBufferDesc(keyID).bufferID];
        for (uint32_t passID = 0; passID < mDesc.getPassCount(); ++passID)
        {
            const auto& passDesc = mDesc.getPassDesc(passID);
            var["gPassDescs"][passID] = passDesc.keyID << 16 | passDesc.keyRadixShift;
        }
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
        pPass->execute(pComputeContext, mDesc.getPassCount() * pPass->getThreadGroupSize().x, 1, 1);
    }

    {
        const auto& pPass = mpOneSweepPass;
        auto var = pPass->getRootVar();
        bindCount(var, count, pCountBuffer, countBufferOffset);
        var["gPassHists"] = resource.pPassHistBuffer;
        var["gIndices"] = resource.pIndexBuffer;

        bool flip = false;

        for (uint passID = 0; passID < mDesc.getPassCount(); ++passID)
        {
            const auto& passDesc = mDesc.getPassDesc(passID);
            const auto& keyDesc = mDesc.getKeyBufferDesc(passDesc.keyID);

            if (passID == keyDesc.firstPassID)
            {
                var["gPayloadBufferCount"] = (uint32_t)keyDesc.payloadBufferIDs.size();
            }

            const auto& getSrcBuffer = [&](uint32_t bufferID) { return flip ? resource.pTempBuffers[bufferID] : pBuffers[bufferID]; };
            const auto& getDstBuffer = [&](uint32_t bufferID) { return flip ? pBuffers[bufferID] : resource.pTempBuffers[bufferID]; };

            var["gPassIdx"] = passID;
            var["gRadixShift"] = passDesc.keyRadixShift;
            var["gSrcKeys"] = getSrcBuffer(keyDesc.bufferID);
            var["gDstKeys"] = getDstBuffer(keyDesc.bufferID);

            for (uint32_t payloadBufferID : keyDesc.payloadBufferIDs)
            {
                var["gSrcPayloads"] = getSrcBuffer(payloadBufferID);
                var["gDstPayloads"] = getDstBuffer(payloadBufferID);
            }

            if constexpr (DispatchType_V == DeviceSortDispatchType::kDirect)
                pPass->execute(pComputeContext, div_round_up(count, kPartitionSize) * pPass->getThreadGroupSize().x, 1, 1);
            else
                pPass->executeIndirect(pComputeContext, resource.pIndirectBuffer.get(), 1 * sizeof(DispatchArguments));

            flip = !flip;
        }
    }
}

template struct DeviceSortResource<DeviceSortDispatchType::kDirect>;
template struct DeviceSortResource<DeviceSortDispatchType::kIndirect>;

template class DeviceSorter<DeviceSortDispatchType::kDirect>;
template class DeviceSorter<DeviceSortDispatchType::kIndirect>;

} // namespace GSGI
