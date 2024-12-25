//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include <ranges>
#include "GS3D.hpp"
#include "../../../Algorithm/MeshSample.hpp"
#include "../../../Algorithm/MeshGSOptimize.hpp"
#include "../../../Scene/GMeshView.hpp"
#include <tbb/parallel_for.h>
#include <boost/random/sobol.hpp>

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
            auto view = GMeshView{pMesh};
            auto sampleResult = MeshSample::sample(
                view,
                [sobolEngine = boost::random::sobol_engine<uint32_t, 32>{4}] mutable
                {
                    uint2 u2;
                    float2 f2;
                    u2.x = sobolEngine();
                    u2.y = sobolEngine();
                    f2.x = float(sobolEngine()) / 4294967296.0f;
                    f2.y = float(sobolEngine()) / 4294967296.0f;
                    return std::tuple{u2, f2};
                },
                mConfig.splatsPerMesh
            );

            static constexpr float kEpsilon = 0.05f, kK = 16.0f;
            float initialScale = kEpsilon * kK * math::sqrt(sampleResult.totalArea / float(mConfig.splatsPerMesh));
            logInfo("area: {}, initialScale: {}", sampleResult.totalArea, initialScale);

            auto bvh = MeshClosestPoint::buildBVH(view);
            splats.resize(splats.size() + mConfig.splatsPerMesh);

            tbb::parallel_for(
                uint32_t{0},
                mConfig.splatsPerMesh,
                [&](uint32_t splatID)
                {
                    splats[splats.size() - splatID - 1] = [&]
                    {
                        const auto& meshPoint = sampleResult.points[splatID];
                        auto result = MeshGSOptimize::run(
                            view,
                            meshPoint,
                            bvh,
                            MeshGSSamplerDefault{std::mt19937{splatID}},
                            {
                                .initialScale = initialScale,
                                .sampleCount = 128,
                                .epsNormal = 0.2f,
                                .epsDistance = 0.2f * initialScale,
                                .epsScale = 1e-6f,
                                .scaleYMaxIteration = 32,
                            }
                        );
                        return GS3DPackedSplat{
                            .barycentrics = meshPoint.barycentrics,
                            .primitiveID = meshPoint.primitiveID,
                            .rotate = float16_t4(result.rotate.x, result.rotate.y, result.rotate.z, result.rotate.w),
                            .scale = float16_t2(result.scaleXY),
                        };
                    }();
                }
            );
        }

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