//
// Created by adamyuan on 12/19/24.
//

#ifndef GSGI_DEVICEUTIL_HPP
#define GSGI_DEVICEUTIL_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

inline uint deviceWaveGetLaneCount(ComputeContext* pComputeContext = nullptr)
{
    static const uint laneCount = [&]
    {
        FALCOR_CHECK(pComputeContext, "pComputeContext must be valid at first run");
        const auto& pDevice = pComputeContext->getDevice();
        auto pPass = ComputePass::create(pDevice, "GaussianGI/Util/WaveOps.cs.slang", "testWaveGetLaneCount");
        auto pBuffer = pDevice->createTypedBuffer<uint32_t>(1, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal);
        pPass->getRootVar()["laneCount"] = pBuffer;
        pPass->execute(pComputeContext, 1, 1, 1);
        return pBuffer->getElement<uint32_t>(0);
    }();
    return laneCount;
}

} // namespace GSGI

#endif // GSGI_DEVICEUTIL_HPP
