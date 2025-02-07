//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include "GS3DIndLightSplat.hpp"

namespace GSGI
{

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
        for (const auto& pMesh : pDefaultStaticScene->getMeshes())
        {
            auto meshSplats = GS3DIndLightSplat::fromMesh(pMesh, mConfig.splatsPerMesh);
            splats.insert(splats.end(), meshSplats.begin(), meshSplats.end());
        }

        mpSplatBuffer = getDevice()->createStructuredBuffer(
            sizeof(GS3DIndLightSplat), //
            splats.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splats.data()
        );
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
            .splatsPerMesh = mConfig.splatsPerMesh,
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