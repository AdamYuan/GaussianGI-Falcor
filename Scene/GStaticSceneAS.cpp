//
// Created by adamyuan on 12/10/24.
//
#include "GStaticScene.hpp"

namespace GSGI
{

namespace
{
struct MeshBLASInfo
{
    std::vector<RtGeometryDesc> geomDescs;
    RtAccelerationStructureBuildInputs buildInputs{};
    uint64_t uncompactedBlasDataSize{};
    ref<Buffer> uncompactedBlasBuffer;
    ref<RtAccelerationStructure> uncompactedBlas;

    uint64_t compactedBlasDataSize{};
    uint64_t compactedBlasDataOffset{};
};
} // namespace

void GStaticScene::buildBLAS(RenderContext* pRenderContext)
{
    std::vector<MeshBLASInfo> meshBlasInfos(getMeshCount());
    uint64_t maxScratchDataSize = 0;

    // Step 1. Prepare
    for (uint meshID = 0; meshID < getMeshCount(); ++meshID)
    {
        const auto& pMesh = mpMeshes[meshID];
        const auto& meshInfo = mMeshInfos[meshID];
        auto& meshBlasInfo = meshBlasInfos[meshID];

        // Geometry Desc
        DeviceAddress indexBufferAddr = mpIndexBuffer->getGpuAddress() + meshInfo.firstIndex * sizeof(GMesh::Index);

        meshBlasInfo.geomDescs.reserve(2);
        if (pMesh->hasNonOpaquePrimitive())
            meshBlasInfo.geomDescs.push_back(
                pMesh->getRTGeometryDesc<RtGeometryFlags::None>(0, indexBufferAddr, mpVertexBuffer->getGpuAddress())
            );
        if (pMesh->hasOpaquePrimitive())
            meshBlasInfo.geomDescs.push_back(
                pMesh->getRTGeometryDesc<RtGeometryFlags::Opaque>(0, indexBufferAddr, mpVertexBuffer->getGpuAddress())
            );

        // Build Inputs
        meshBlasInfo.buildInputs = {
            .kind = RtAccelerationStructureKind::BottomLevel,
            .flags = RtAccelerationStructureBuildFlags::AllowCompaction | RtAccelerationStructureBuildFlags::PreferFastTrace,
            .descCount = (uint)meshBlasInfo.geomDescs.size(),
            .geometryDescs = meshBlasInfo.geomDescs.data(),
        };

        // Uncompacted Sizes
        {
            RtAccelerationStructurePrebuildInfo preBuildInfo =
                RtAccelerationStructure::getPrebuildInfo(getDevice().get(), meshBlasInfo.buildInputs);
            FALCOR_ASSERT(preBuildInfo.resultDataMaxSize > 0);
            meshBlasInfo.uncompactedBlasDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.resultDataMaxSize);
            auto scratchDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.scratchDataSize);
            maxScratchDataSize = math::max(maxScratchDataSize, scratchDataSize);
        }

        // Uncompacted BLAS (not built yet)
        {
            meshBlasInfo.uncompactedBlasBuffer = getDevice()->createBuffer(
                meshBlasInfo.uncompactedBlasDataSize, ResourceBindFlags::AccelerationStructure, MemoryType::DeviceLocal
            );

            RtAccelerationStructure::Desc desc{};
            desc.setKind(RtAccelerationStructureKind::BottomLevel);
            desc.setBuffer(meshBlasInfo.uncompactedBlasBuffer, 0, meshBlasInfo.uncompactedBlasDataSize);
            meshBlasInfo.uncompactedBlas = RtAccelerationStructure::create(getDevice(), desc);
        }
    }

    ref<RtAccelerationStructurePostBuildInfoPool> compactedSizeInfoPool = RtAccelerationStructurePostBuildInfoPool::create(
        getDevice().get(),
        {
            .queryType = RtAccelerationStructurePostBuildInfoQueryType::CompactedSize,
            .elementCount = getMeshCount(),
        }
    );
    compactedSizeInfoPool->reset(pRenderContext);

    // Step 2. Build
    uint64_t totalCompactedBlasDataSize = 0;
    auto scratchBuffer = getDevice()->createBuffer(maxScratchDataSize, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal);
    for (uint meshID = 0; meshID < getMeshCount(); ++meshID)
    {
        auto& meshBlasInfo = meshBlasInfos[meshID];

        RtAccelerationStructure::BuildDesc buildDesc = {
            .inputs = meshBlasInfo.buildInputs,
            .source = nullptr,
            .dest = meshBlasInfo.uncompactedBlas.get(),
            .scratchData = scratchBuffer->getGpuAddress(),
        };
        RtAccelerationStructurePostBuildInfoDesc postBuildInfoDesc = {
            .type = RtAccelerationStructurePostBuildInfoQueryType::CompactedSize,
            .pool = compactedSizeInfoPool.get(),
            .index = meshID,
        };

        pRenderContext->buildAccelerationStructure(buildDesc, 1, &postBuildInfoDesc);
        pRenderContext->submit(true);

        meshBlasInfo.compactedBlasDataSize = compactedSizeInfoPool->getElement(pRenderContext, meshID);
        meshBlasInfo.compactedBlasDataSize = align_to(kAccelerationStructureByteAlignment, meshBlasInfo.compactedBlasDataSize);
        meshBlasInfo.compactedBlasDataOffset = totalCompactedBlasDataSize;
        totalCompactedBlasDataSize += meshBlasInfo.compactedBlasDataSize;
    }

    // Step 3: Compact
    mpBLASBuffer = getDevice()->createBuffer(totalCompactedBlasDataSize, ResourceBindFlags::AccelerationStructure, MemoryType::DeviceLocal);
    mpMeshBLASs.resize(getMeshCount());
    for (uint meshID = 0; meshID < getMeshCount(); ++meshID)
    {
        auto& meshBlasInfo = meshBlasInfos[meshID];

        RtAccelerationStructure::Desc desc{};
        desc.setKind(RtAccelerationStructureKind::BottomLevel);
        desc.setBuffer(mpBLASBuffer, meshBlasInfo.compactedBlasDataOffset, meshBlasInfo.compactedBlasDataSize);
        mpMeshBLASs[meshID] = RtAccelerationStructure::create(getDevice(), desc);

        pRenderContext->copyAccelerationStructure(
            mpMeshBLASs[meshID].get(), meshBlasInfo.uncompactedBlas.get(), RenderContext::RtAccelerationStructureCopyMode::Compact
        );
    }
    pRenderContext->submit(true);
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