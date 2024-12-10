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
    uint64_t scratchDataSize{};
    uint64_t uncompactedBlasDataSize{};
    ref<Buffer> uncompactedBlasBuffer;
    ref<RtAccelerationStructure> uncompactedBlas;

    uint64_t compactedBlasDataSize{};
    uint64_t compactedBlasDataOffset{};
};
} // namespace

void GStaticScene::buildBLAS(RenderContext* pRenderContext)
{
    std::vector<MeshBLASInfo> meshBlasInfos(mMeshViews.size());
    uint64_t maxScratchDataSize = 0;

    // Step 1. Prepare
    for (uint meshID = 0; meshID < mMeshViews.size(); ++meshID)
    {
        const auto& meshView = mMeshViews[meshID];
        auto& meshBlasInfo = meshBlasInfos[meshID];

        // Geometry Desc
        DeviceAddress indexBufferAddr = mpIndexBuffer->getGpuAddress() + meshView.info.firstIndex * sizeof(GMesh::Index);
        auto optOpaqueGeomDesc =
            meshView.pMesh->getRTGeometryDesc(RtGeometryFlags::Opaque, 0, indexBufferAddr, mpVertexBuffer->getGpuAddress());
        auto optNonOpaqueGeomDesc =
            meshView.pMesh->getRTGeometryDesc(RtGeometryFlags::None, 0, indexBufferAddr, mpVertexBuffer->getGpuAddress());
        if (optOpaqueGeomDesc)
            meshBlasInfo.geomDescs.push_back(*optOpaqueGeomDesc);
        if (optNonOpaqueGeomDesc)
            meshBlasInfo.geomDescs.push_back(*optNonOpaqueGeomDesc);

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
            meshBlasInfo.scratchDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.scratchDataSize);
            maxScratchDataSize = math::max(maxScratchDataSize, meshBlasInfo.scratchDataSize);
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
            .elementCount = (uint32_t)meshBlasInfos.size(),
        }
    );
    compactedSizeInfoPool->reset(pRenderContext);

    // Step 2. Build
    uint64_t totalCompactedBlasDataSize = 0;
    auto scratchBuffer = getDevice()->createBuffer(maxScratchDataSize, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal);
    for (uint meshID = 0; meshID < mMeshViews.size(); ++meshID)
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
    mpMeshBLASs.resize(meshBlasInfos.size());
    for (uint meshID = 0; meshID < mMeshViews.size(); ++meshID)
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

void GStaticScene::buildTLAS(RenderContext* pRenderContext) {}

} // namespace GSGI