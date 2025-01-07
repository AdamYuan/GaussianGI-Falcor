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
        ProgramDesc::ShaderModule shaderModule;
        shaderModule.addString(R"(
RWBuffer<uint> laneCount;
[numthreads(1, 1, 1)]
void csMain() { laneCount[0] = WaveGetLaneCount(); }
)");
        ProgramDesc desc = {};
        desc.addShaderModule(shaderModule).csEntry("csMain");
        auto pPass = ComputePass::create(pDevice, desc);
        auto pBuffer = pDevice->createTypedBuffer<uint32_t>(1, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal);
        pPass->getRootVar()["laneCount"] = pBuffer;
        pPass->execute(pDevice->getRenderContext(), 1, 1, 1);
        return pBuffer->getElement<uint32_t>(0);
    }();
    return laneCount;
}

} // namespace GSGI
