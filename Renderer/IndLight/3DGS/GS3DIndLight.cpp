//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include <ranges>
#include "GS3D.hpp"
#include "../../../Algorithm/MeshSample.hpp"
#include "../../../Algorithm/MeshGSOptimize.hpp"
#include "../../../Scene/GMeshView.hpp"

namespace GSGI
{

// static MeshClosestPointBVH meshBvh;

GS3DIndLight::GS3DIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    mpMiscRenderer = make_ref<GS3DMiscRenderer>(getDevice());
}

void GS3DIndLight::update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
{
    mpStaticScene = pDefaultStaticScene;
    if (isSceneChanged)
    {
        std::vector<GS3DPackedSplat> splats;
        for (const auto& pMesh : pDefaultStaticScene->getMeshes())
        {
            auto sampler = MeshSamplerDefault<std::mt19937>{};
            auto sampleResult = MeshSample::sample(GMeshView{pMesh}, sampler, mConfig.splatsPerMesh);
            static constexpr float kEpsilon = 0.01f, kK = 32.0f;
            float initialScale = kEpsilon * kK * math::sqrt(sampleResult.totalArea / float(mConfig.splatsPerMesh));
            logInfo("area: {}, initialScale: {}", sampleResult.totalArea, initialScale);
            auto meshSplats =
                sampleResult.points | std::views::transform(
                                          [&](const MeshPoint& meshPoint) -> GS3DPackedSplat
                                          {
                                              auto result = MeshGSOptimize::run(
                                                  GMeshView{pMesh},
                                                  meshPoint,
                                                  {
                                                      .initialScale = initialScale,
                                                  }
                                              );
                                              return GS3DPackedSplat{
                                                  .barycentrics = meshPoint.barycentrics,
                                                  .primitiveID = meshPoint.primitiveID,
                                                  .rotate = float16_t4(result.rotate.x, result.rotate.y, result.rotate.z, result.rotate.w),
                                                  .scale = float16_t2(result.scaleXY),
                                              };
                                          }
                                      );
            splats.insert(splats.end(), meshSplats.begin(), meshSplats.end());
        }

        // meshBvh = MeshClosestPoint::buildBVH(GMeshView{mpStaticScene->getMeshes()[0]});

        mpSplatBuffer = getDevice()->createStructuredBuffer(
            sizeof(GS3DPackedSplat), //
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