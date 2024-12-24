//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include <ranges>
#include "GS3D.hpp"
#include "../../../Algorithm/MeshSample.hpp"
#include "../../../Scene/GMeshView.hpp"

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
        std::vector<GS3DSplat> splats;
        for (const auto& pMesh : pDefaultStaticScene->getMeshes())
        {
            auto sampler = MeshSamplerDefault<std::mt19937>{};
            auto view = GMeshView::make(pMesh);
            auto sampleResult = MeshSample::sample(view, sampler, mConfig.splatsPerMesh);
            static constexpr float kEpsilon = 0.01f, kK = 16.0f;
            float initialScale = kEpsilon * kK * math::sqrt(sampleResult.totalArea) / float(mConfig.splatsPerMesh);
            auto meshSplats = sampleResult.points | std::views::transform(
                                                        [&](const MeshPoint& meshPoint) -> GS3DSplat
                                                        {
                                                            return GS3DSplat{
                                                                .barycentrics = meshPoint.barycentrics,
                                                                .primitiveID = meshPoint.primitiveID,
                                                                .rotate = float16_t4(0.0f, 0.0f, 0.0f, 1.0f),
                                                                .scale = float16_t2(initialScale, initialScale),
                                                            };
                                                        }
                                                    );
            splats.insert(splats.end(), meshSplats.begin(), meshSplats.end());
        }

        mpSplatBuffer = getDevice()->createStructuredBuffer(
            sizeof(GS3DSplat), //
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
}

} // namespace GSGI