//
// Created by adamyuan on 12/20/24.
//
#include "DeviceUtil.hpp"

namespace GSGI
{

uint deviceWaveGetLaneCount(const ref<Device>& pDevice)
{
    static const uint laneCount = [&]
    {
        auto pPass = ComputePass::create(pDevice, "GaussianGI/Util/WaveOps.cs.slang", "testWaveGetLaneCount");
        auto pBuffer = pDevice->createTypedBuffer<uint32_t>(1, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal);
        pPass->getRootVar()["laneCount"] = pBuffer;
        pPass->execute(pDevice->getRenderContext(), 1, 1, 1);
        return pBuffer->getElement<uint32_t>(0);
    }();
    return laneCount;
}

} // namespace GSGI
