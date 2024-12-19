//
// Created by adamyuan on 12/19/24.
//
#include "DeviceSorter.hpp"

#include "../../Util/DeviceUtil.hpp"

namespace GSGI
{

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
DeviceSorterResource<Type_V, DispatchType_V> DeviceSorterResource<Type_V, DispatchType_V>::create(const ref<Device>& pDevice, uint maxCount)
{
    FALCOR_CHECK(maxCount <= DeviceSorterProperty::kMaxSize, "maxCount should not exceed DeviceSorterProperty::kMaxSize");
    DeviceSorterResource res = {.maxCount = maxCount};
    res.pTempKeyBuffer = pDevice->createStructuredBuffer(sizeof(uint), maxCount);
    res.pGlobalHistBuffer =
        pDevice->createStructuredBuffer(sizeof(uint), DeviceSorterProperty::kRadix * DeviceSorterProperty::kRadixPasses);
    res.pPassHistBuffer = pDevice->createStructuredBuffer(
        sizeof(uint),
        DeviceSorterProperty::kRadix * DeviceSorterProperty::kRadixPasses * div_round_up(maxCount, DeviceSorterProperty::kPartitionSize)
    );
    res.pIndexBuffer = pDevice->createStructuredBuffer(sizeof(uint), DeviceSorterProperty::kRadixPasses);
    if constexpr (Type_V == DeviceSortType::kPair)
        res.pTempPayloadBuffer = pDevice->createStructuredBuffer(sizeof(uint), maxCount);
    if constexpr (DispatchType_V == DeviceSortDispatchType::kIndirect)
    {
        uint zeros[6] = {};
        res.pIndirectBuffer = pDevice->createStructuredBuffer(
            sizeof(uint),
            6,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::IndirectArg,
            MemoryType::DeviceLocal,
            zeros
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
        var["b_numKeys"].setSrv(pCountBuffer->getSRV(countBufferOffset, sizeof(uint32_t)));
    else
        var["e_numKeys"] = count;
}

template<DeviceSortType Type_V, DeviceSortDispatchType DispatchType_V>
DeviceSorter<Type_V, DispatchType_V>::DeviceSorter(ref<Device> pDevice) : GDeviceObject<DeviceSorter>(std::move(pDevice))
{
    constexpr const char* kShaderPath = "GaussianGI/Algorithm/GPUSorting/OneSweep.cs.slang";
    DefineList defList{};
    defList.add("SORT_PAIRS", Type_V == DeviceSortType::kPair ? "1" : "0");
    defList.add("SORT_INDIRECT", DispatchType_V == DeviceSortDispatchType::kIndirect ? "1" : "0");
    defList.add("LANES_PER_WAVE", fmt::to_string(deviceWaveGetLaneCount()));
    mpInitSweepPass = ComputePass::create(pDevice, kShaderPath, "csInitSweep", defList);
    mpGlobalHistPass = ComputePass::create(pDevice, kShaderPath, "csGlobalHistogram", defList);
    mpScanPass = ComputePass::create(pDevice, kShaderPath, "csScan", defList);
    mpDigitBinningPass = ComputePass::create(pDevice, kShaderPath, "csDigitBinning", defList);
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
        const auto& pPass = mpInitSweepPass;
        auto var = pPass->getRootVar();
        var["b_passHist"] = resource.pPassHistBuffer;
        var["b_globalHist"] = resource.pGlobalHistBuffer;
        var["b_index"] = resource.pIndexBuffer;
        bindCount(var, count, pCountBuffer, countBufferOffset);
        if constexpr (DispatchType_V == DeviceSortDispatchType::kIndirect)
            var["b_indirect"] = resource.pIndirectBuffer;
        pPass->execute(pComputeContext, 256 * pPass->getThreadGroupSize().x, 1, 1);
    }

    {
        const auto& pPass = mpGlobalHistPass;
        auto var = pPass->getRootVar();
        var["b_sort"] = pKeyBuffer;
        var["b_globalHist"] = resource.pGlobalHistBuffer;
        bindCount(var, count, pCountBuffer, countBufferOffset);
        if constexpr (DispatchType_V == DeviceSortDispatchType::kDirect)
            pPass->execute(
                pComputeContext, div_round_up(count, DeviceSorterProperty::kGlobalHistPartSize) * pPass->getThreadGroupSize().x, 1, 1
            );
        else
            pPass->executeIndirect(pComputeContext, resource.pIndirectBuffer.get(), 0 * sizeof(uint32_t));
    }

    {
        const auto& pPass = mpScanPass;
        auto var = pPass->getRootVar();
        var["b_passHist"] = resource.pPassHistBuffer;
        var["b_globalHist"] = resource.pGlobalHistBuffer;
        bindCount(var, count, pCountBuffer, countBufferOffset);
        pPass->execute(pComputeContext, DeviceSorterProperty::kRadixPasses * pPass->getThreadGroupSize().x, 1, 1);
    }

    {
        const auto& pPass = mpDigitBinningPass;
        auto var = pPass->getRootVar();
        var["b_passHist"] = resource.pPassHistBuffer;
        var["b_index"] = resource.pIndexBuffer;

        bool flip = false;
        for (uint radixShift = 0; radixShift < 32; radixShift += 8, flip = !flip)
        {
            var["e_radixShift"] = radixShift;
            var["b_sort"] = flip ? resource.pTempKeyBuffer : pKeyBuffer;
            var["b_alt"] = flip ? pKeyBuffer : resource.pTempKeyBuffer;

            if constexpr (Type_V == DeviceSortType::kPair)
            {
                var["b_sortPayload"] = flip ? resource.pTempPayloadBuffer : pPayloadBuffer;
                var["b_altPayload"] = flip ? pPayloadBuffer : resource.pTempPayloadBuffer;
            }

            if constexpr (DispatchType_V == DeviceSortDispatchType::kDirect)
                pPass->execute(
                    pComputeContext, div_round_up(count, DeviceSorterProperty::kPartitionSize) * pPass->getThreadGroupSize().x, 1, 1
                );
            else
                pPass->executeIndirect(pComputeContext, resource.pIndirectBuffer.get(), 3 * sizeof(uint32_t));
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
