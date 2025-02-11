//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include "GS3DIndLightSplat.hpp"
#include "GS3DIndLightAlgo.hpp"
#include "../../../Algorithm/MeshVHBVH.hpp"

namespace GSGI
{

namespace
{
constexpr uint32_t kDefaultSplatsPerMesh = 65536;
}

GS3DIndLight::GS3DIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    mpMiscRenderer = make_ref<GS3DMiscRenderer>(getDevice());
}

void GS3DIndLight::update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
{
    mpStaticScene = pDefaultStaticScene;
    if (isSceneChanged)
    {
        std::vector<GS3DIndLightSplat> splats;
        std::vector<uint32_t> meshFirstSplatIdx;
        meshFirstSplatIdx.reserve(mpStaticScene->getMeshCount());
        for (const auto& pMesh : mpStaticScene->getMeshes())
        {
            auto meshView = GMeshView{pMesh};
            MeshBVH<AABB> meshBVH;
            auto meshSplats = GS3DIndLightSplat::loadMesh(pMesh);
            if (meshSplats.empty())
            {
                if (meshBVH.isEmpty())
                    meshBVH = MeshBVH<AABB>::build<MeshVHBVHBuilder>(meshView);
                meshSplats = GS3DIndLightAlgo::getSplatsFromMeshFallback(meshView, meshBVH, kDefaultSplatsPerMesh);
                GS3DIndLightSplat::persistMesh(pMesh, meshSplats);
            }
            meshFirstSplatIdx.push_back(splats.size());
            splats.insert(splats.end(), meshSplats.begin(), meshSplats.end());

            /* {
                if (meshBVH.isEmpty())
                    meshBVH = MeshBVH<AABB>::build<MeshVHBVHBuilder>(meshView);

                std::vector<std::vector<uint32_t>> primSplatIDs =
                    GS3DIndLightAlgo::getPrimitiveIntersectedSplatIDs(meshView, meshBVH, meshSplats);

                uint32_t greater16Cnt = 0, greater8Cnt = 0, equal0Cnt = 0;
                for (uint i = 0; i < pMesh->getPrimitiveCount(); ++i)
                {
                    if (primSplatIDs[i].size() > 16)
                        ++greater16Cnt;
                    if (primSplatIDs[i].size() > 8)
                        ++greater8Cnt;
                    if (primSplatIDs[i].empty())
                        ++equal0Cnt;
                }
                fmt::println(">16: {}, >8: {}, =0: {}", greater16Cnt, greater8Cnt, equal0Cnt);
            } */
        }

        std::vector<uint32_t> splatDescs;

        for (uint32_t instanceID = 0; instanceID < mpStaticScene->getInstanceCount(); ++instanceID)
        {
            const auto& instanceInfo = mpStaticScene->getInstanceInfos()[instanceID];
            uint32_t meshID = instanceInfo.meshID;
            uint32_t firstSplatIdx = meshFirstSplatIdx[meshID];
            uint32_t splatCount =
                (meshID == mpStaticScene->getMeshCount() - 1 ? splats.size() : meshFirstSplatIdx[instanceInfo.meshID + 1]) - firstSplatIdx;

            for (uint32_t splatOfst = 0; splatOfst < splatCount; ++splatOfst)
                splatDescs.push_back((firstSplatIdx + splatOfst) | (instanceID << 24u));

            static_assert(GStaticScene::kMaxInstanceCount <= 256);
        }

        mpSplatBuffer = getDevice()->createStructuredBuffer(
            sizeof(GS3DIndLightSplat), //
            splats.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splats.data()
        );
        mpSplatDescBuffer = getDevice()->createStructuredBuffer(
            sizeof(uint32_t), //
            splatDescs.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splatDescs.data()
        );
        mSplatCount = splatDescs.size();
    }
}

void GS3DIndLight::draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture) {}

void GS3DIndLight::drawMisc(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    mpMiscRenderer->draw(
        pRenderContext,
        pTargetFbo,
        {
            .pStaticScene = mpStaticScene,
            .pSplatBuffer = mpSplatBuffer,
            .pSplatDescBuffer = mpSplatDescBuffer,
            .splatCount = mSplatCount,
        }
    );
}

void GS3DIndLight::renderUIImpl(Gui::Widgets& widget)
{
    if (auto g = widget.group("Misc", true))
        mpMiscRenderer->renderUI(g);

    /* auto result = MeshClosestPoint::query(
        GMeshView{mpStaticScene->getMeshes()[0]}, meshBvh, mpStaticScene->getScene()->getCamera()->getPosition(), 1.0f
    );
    widget.text(fmt::format("dist = {}, id = {}", math::sqrt(result.dist2), result.optPrimitiveID)); */
}

} // namespace GSGI