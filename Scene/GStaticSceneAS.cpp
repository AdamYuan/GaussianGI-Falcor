//
// Created by adamyuan on 12/10/24.
//
#include "GStaticScene.hpp"

#include "../Util/BLASUtil.hpp"
#include "../Util/TLASUtil.hpp"

namespace GSGI
{

void GStaticScene::buildBLAS(RenderContext* pRenderContext)
{
    std::vector<BLASBuildDesc> blasBuildDescs(getMeshCount());

    for (uint meshID = 0; meshID < getMeshCount(); ++meshID)
    {
        const auto& pMesh = mpMeshes[meshID];
        const auto& meshInfo = mMeshInfos[meshID];
        auto& blasBuildDesc = blasBuildDescs[meshID];

        // Geometry Desc
        DeviceAddress indexBufferAddr = mpIndexBuffer->getGpuAddress() + meshInfo.firstIndex * sizeof(GMesh::Index);

        blasBuildDesc.geomDescs.reserve(2);
        if (pMesh->hasNonOpaquePrimitive())
            blasBuildDesc.geomDescs.push_back(
                pMesh->getRTGeometryDesc<RtGeometryFlags::None>(0, indexBufferAddr, mpVertexBuffer->getGpuAddress())
            );
        if (pMesh->hasOpaquePrimitive())
            blasBuildDesc.geomDescs.push_back(
                pMesh->getRTGeometryDesc<RtGeometryFlags::Opaque>(0, indexBufferAddr, mpVertexBuffer->getGpuAddress())
            );
    }

    mpMeshBLASs = BLASBuilder::build(pRenderContext, blasBuildDescs);
}

void GStaticScene::buildTLAS(RenderContext* pRenderContext)
{
    TLASBuildDesc tlasBuildDesc;
    tlasBuildDesc.instanceDescs.reserve(getInstanceCount());

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
        tlasBuildDesc.instanceDescs.push_back(instanceDesc);
    }

    mpTLAS = TLASBuilder::build(pRenderContext, tlasBuildDesc);
}

} // namespace GSGI