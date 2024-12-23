//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include <ranges>
#include "GS3D.hpp"
#include "../../../Algorithm/MeshSample.hpp"
#include "../../../Scene/GMeshView.hpp"
#include "../../../Util/ShaderUtil.hpp"

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
            auto meshSamples = MeshSample::sample(view, sampler, mConfig.splatsPerMesh);
            auto meshSplats = meshSamples | std::views::transform(
                                                [](const MeshPoint& meshPoint) -> GS3DSplat
                                                {
                                                    return GS3DSplat{
                                                        .barycentrics = meshPoint.barycentrics,
                                                        .primitiveID = meshPoint.primitiveID,
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

void GS3DIndLight::renderUIImpl(Gui::Widgets& widget) {}

} // namespace GSGI