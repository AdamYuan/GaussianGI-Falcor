//
// Created by adamyuan on 2/18/25.
//

#include "BLASUtil.hpp"

namespace GSGI
{

namespace
{
struct BLASBuildInfo
{
    RtAccelerationStructureBuildInputs buildInputs{};
    uint64_t uncompactedBlasDataSize{};
    ref<Buffer> uncompactedBlasBuffer;
    ref<RtAccelerationStructure> uncompactedBlas;

    uint64_t compactedBlasDataSize{};
    uint64_t compactedBlasDataOffset{};
};
} // namespace

std::vector<ref<RtAccelerationStructure>> BLASBuilder::build(RenderContext* pRenderContext, std::span<const BLASBuildDesc> buildDescs)
{
    auto pDevice = pRenderContext->getDevice();

    uint count = buildDescs.size();
    std::vector<BLASBuildInfo> buildInfos(count);
    uint64_t maxScratchDataSize = 0;

    // Step 1. Prepare
    for (uint blasID = 0; blasID < count; ++blasID)
    {
        const auto& buildDesc = buildDescs[blasID];
        auto& buildInfo = buildInfos[blasID];

        // Build Inputs
        buildInfo.buildInputs = {
            .kind = RtAccelerationStructureKind::BottomLevel,
            .flags = RtAccelerationStructureBuildFlags::AllowCompaction | RtAccelerationStructureBuildFlags::PreferFastTrace,
            .descCount = (uint)buildDesc.geomDescs.size(),
            .geometryDescs = buildDesc.geomDescs.data(),
        };

        // Uncompacted Sizes
        {
            RtAccelerationStructurePrebuildInfo preBuildInfo =
                RtAccelerationStructure::getPrebuildInfo(pDevice.get(), buildInfo.buildInputs);
            FALCOR_ASSERT(preBuildInfo.resultDataMaxSize > 0);
            buildInfo.uncompactedBlasDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.resultDataMaxSize);
            auto scratchDataSize = align_to(kAccelerationStructureByteAlignment, preBuildInfo.scratchDataSize);
            maxScratchDataSize = math::max(maxScratchDataSize, scratchDataSize);
        }

        // Uncompacted BLAS (not built yet)
        {
            buildInfo.uncompactedBlasBuffer =
                pDevice->createBuffer(buildInfo.uncompactedBlasDataSize, ResourceBindFlags::AccelerationStructure, MemoryType::DeviceLocal);

            RtAccelerationStructure::Desc desc{};
            desc.setKind(RtAccelerationStructureKind::BottomLevel);
            desc.setBuffer(buildInfo.uncompactedBlasBuffer, 0, buildInfo.uncompactedBlasDataSize);
            buildInfo.uncompactedBlas = RtAccelerationStructure::create(pDevice, desc);
        }
    }

    ref<RtAccelerationStructurePostBuildInfoPool> compactedSizeInfoPool = RtAccelerationStructurePostBuildInfoPool::create(
        pDevice.get(),
        {
            .queryType = RtAccelerationStructurePostBuildInfoQueryType::CompactedSize,
            .elementCount = count,
        }
    );
    compactedSizeInfoPool->reset(pRenderContext);

    // Step 2. Build
    uint64_t totalCompactedBlasDataSize = 0;
    auto scratchBuffer = pDevice->createBuffer(maxScratchDataSize, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal);
    for (uint blasID = 0; blasID < count; ++blasID)
    {
        auto& buildInfo = buildInfos[blasID];

        RtAccelerationStructure::BuildDesc buildDesc = {
            .inputs = buildInfo.buildInputs,
            .source = nullptr,
            .dest = buildInfo.uncompactedBlas.get(),
            .scratchData = scratchBuffer->getGpuAddress(),
        };
        RtAccelerationStructurePostBuildInfoDesc postBuildInfoDesc = {
            .type = RtAccelerationStructurePostBuildInfoQueryType::CompactedSize,
            .pool = compactedSizeInfoPool.get(),
            .index = blasID,
        };

        pRenderContext->buildAccelerationStructure(buildDesc, 1, &postBuildInfoDesc);
        pRenderContext->submit(true);

        buildInfo.compactedBlasDataSize = compactedSizeInfoPool->getElement(pRenderContext, blasID);
        buildInfo.compactedBlasDataSize = align_to(kAccelerationStructureByteAlignment, buildInfo.compactedBlasDataSize);
        buildInfo.compactedBlasDataOffset = totalCompactedBlasDataSize;
        totalCompactedBlasDataSize += buildInfo.compactedBlasDataSize;
    }

    // Step 3: Compact
    std::vector<ref<RtAccelerationStructure>> pBLASs(count);
    auto pBLASBuffer = pDevice->createBuffer(totalCompactedBlasDataSize, ResourceBindFlags::AccelerationStructure, MemoryType::DeviceLocal);

    for (uint blasID = 0; blasID < count; ++blasID)
    {
        const auto& buildInfo = buildInfos[blasID];

        RtAccelerationStructure::Desc desc{};
        desc.setKind(RtAccelerationStructureKind::BottomLevel);
        desc.setBuffer(pBLASBuffer, buildInfo.compactedBlasDataOffset, buildInfo.compactedBlasDataSize);
        pBLASs[blasID] = RtAccelerationStructure::create(pDevice, desc);

        pRenderContext->copyAccelerationStructure(
            pBLASs[blasID].get(), buildInfo.uncompactedBlas.get(), RenderContext::RtAccelerationStructureCopyMode::Compact
        );
    }
    pRenderContext->submit(true);

    return pBLASs;
}

} // namespace GSGI