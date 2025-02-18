//
// Created by adamyuan on 12/10/24.
//
#include "GStaticScene.hpp"

#include "../Util/BLASUtil.hpp"

namespace GSGI
{

void GStaticScene::buildBLAS(RenderContext* pRenderContext)
{
    std::vector<BLASBuildInput> blasBuildInputs(getMeshCount());

    for (uint meshID = 0; meshID < getMeshCount(); ++meshID)
    {
        const auto& pMesh = mpMeshes[meshID];
        const auto& meshInfo = mMeshInfos[meshID];
        auto& blasBuildInput = blasBuildInputs[meshID];

        // Geometry Desc
        DeviceAddress indexBufferAddr = mpIndexBuffer->getGpuAddress() + meshInfo.firstIndex * sizeof(GMesh::Index);

        blasBuildInput.geomDescs.reserve(2);
        if (pMesh->hasNonOpaquePrimitive())
            blasBuildInput.geomDescs.push_back(
                pMesh->getRTGeometryDesc<RtGeometryFlags::None>(0, indexBufferAddr, mpVertexBuffer->getGpuAddress())
            );
        if (pMesh->hasOpaquePrimitive())
            blasBuildInput.geomDescs.push_back(
                pMesh->getRTGeometryDesc<RtGeometryFlags::Opaque>(0, indexBufferAddr, mpVertexBuffer->getGpuAddress())
            );
    }

    mpMeshBLASs = BLASBuilder::build(pRenderContext, blasBuildInputs);
}

void GStaticScene::buildTLAS(RenderContext* pRenderContext)
{
    // Step 1: Prepare
    RtAccelerationStructureBuildInputs buildInputs = {
        .kind = RtAccelerationStructureKind::TopLevel,
        .flags = RtAccelerationStructureBuildFlags::PreferFastTrace,
        .descCount = (uint)getInstanceCount(),
    };

    // instance descriptors
    {
        std::vector<RtInstanceDesc> instanceDescs;
        instanceDescs.reserve(getInstanceCount());

        for (uint instanceID = 0; instanceID < getInstanceCount(); ++instanceID)
        {
            const auto& instanceInfo = mInstanceInfos[instanceID];
            auto instanceDesc = RtInstanceDesc{
                .instanceID = mpMeshes[instanceInfo.meshID]->getData().firstOpaquePrimitiveID, // Custom InstanceID
                .instanceMask = 0xFF,
                .instanceContributionToHitGroupIndex = 0,
                .flags = RtGeometryInstanceFlags::TriangleFacingCullDisable,
                .accelerationStructure = mpMeshBLASs[instanceInfo.meshID]->getGpuAddress(),
            };
            auto transform3x4 = instanceInfo.transform.getMatrix3x4();
            std::memcpy(instanceDesc.transform, &transform3x4, sizeof(instanceDesc.transform));
            static_assert(sizeof(instanceDesc.transform) == sizeof(transform3x4));
            instanceDescs.push_back(instanceDesc);
        }

        GpuMemoryHeap::Allocation allocation =
            getDevice()->getUploadHeap()->allocate(buildInputs.descCount * sizeof(RtInstanceDesc), sizeof(RtInstanceDesc));
        std::copy(instanceDescs.begin(), instanceDescs.end(), reinterpret_cast<RtInstanceDesc*>(allocation.pData));
        buildInputs.instanceDescs = allocation.getGpuAddress();
        getDevice()->getUploadHeap()->release(allocation); // Deferred release
    }

    // query sizes
    uint64_t tlasDataSize, scratchDataSize;
    {
        RtAccelerationStructurePrebuildInfo preBuildInfo = RtAccelerationStructure::getPrebuildInfo(getDevice().get(), buildInputs);
        FALCOR_ASSERT(preBuildInfo.resultDataMaxSize > 0);
        tlasDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.resultDataMaxSize);
        scratchDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.scratchDataSize);
    }

    // create TLAS
    mpTLASBuffer = getDevice()->createBuffer(tlasDataSize, ResourceBindFlags::AccelerationStructure, MemoryType::DeviceLocal);
    {
        RtAccelerationStructure::Desc desc{};
        desc.setKind(RtAccelerationStructureKind::TopLevel);
        desc.setBuffer(mpTLASBuffer, 0, tlasDataSize);
        mpTLAS = RtAccelerationStructure::create(getDevice(), desc);
    }

    // scratch buffer
    auto scratchBuffer = getDevice()->createBuffer(scratchDataSize, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal);

    // Step 2: Build
    RtAccelerationStructure::BuildDesc buildDesc = {
        .inputs = buildInputs,
        .source = nullptr,
        .dest = mpTLAS.get(),
        .scratchData = scratchBuffer->getGpuAddress(),
    };
    pRenderContext->buildAccelerationStructure(buildDesc, 0, nullptr);
    pRenderContext->submit(true);
}

} // namespace GSGI