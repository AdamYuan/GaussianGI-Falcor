//
// Created by adamyuan on 2/18/25.
//

#include "TLASUtil.hpp"

namespace GSGI
{

ref<RtAccelerationStructure> TLASBuilder::build(RenderContext* pRenderContext, const TLASBuildDesc& buildDesc)
{
    auto pDevice = pRenderContext->getDevice();

    uint instanceCount = buildDesc.instanceDescs.size();

    // Step 1: Prepare
    RtAccelerationStructureBuildInputs buildInputs = {
        .kind = RtAccelerationStructureKind::TopLevel,
        .flags = RtAccelerationStructureBuildFlags::PreferFastTrace,
        .descCount = instanceCount,
    };

    // instance descriptors
    {
        GpuMemoryHeap::Allocation allocation =
            pDevice->getUploadHeap()->allocate(buildInputs.descCount * sizeof(RtInstanceDesc), sizeof(RtInstanceDesc));
        std::copy(buildDesc.instanceDescs.begin(), buildDesc.instanceDescs.end(), reinterpret_cast<RtInstanceDesc*>(allocation.pData));
        buildInputs.instanceDescs = allocation.getGpuAddress();
        pDevice->getUploadHeap()->release(allocation); // Deferred release
    }

    // query sizes
    uint64_t tlasDataSize, scratchDataSize;
    {
        RtAccelerationStructurePrebuildInfo preBuildInfo = RtAccelerationStructure::getPrebuildInfo(pDevice.get(), buildInputs);
        FALCOR_ASSERT(preBuildInfo.resultDataMaxSize > 0);
        tlasDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.resultDataMaxSize);
        scratchDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.scratchDataSize);
    }

    // create TLAS
    auto pTLASBuffer = pDevice->createBuffer(tlasDataSize, ResourceBindFlags::AccelerationStructure, MemoryType::DeviceLocal);

    RtAccelerationStructure::Desc desc{};
    desc.setKind(RtAccelerationStructureKind::TopLevel);
    desc.setBuffer(pTLASBuffer, 0, tlasDataSize);
    auto pTLAS = RtAccelerationStructure::create(pDevice, desc);

    // scratch buffer
    auto scratchBuffer = pDevice->createBuffer(scratchDataSize, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal);

    // Step 2: Build
    RtAccelerationStructure::BuildDesc rtBuildDesc = {
        .inputs = buildInputs,
        .source = nullptr,
        .dest = pTLAS.get(),
        .scratchData = scratchBuffer->getGpuAddress(),
    };
    pRenderContext->buildAccelerationStructure(rtBuildDesc, 0, nullptr);
    pRenderContext->submit(true);

    return pTLAS;
}

} // namespace GSGI