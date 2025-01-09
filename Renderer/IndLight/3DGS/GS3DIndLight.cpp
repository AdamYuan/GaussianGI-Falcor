//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include "GS3DMiscData.hpp"
#include "../../../Util/SerializeUtil.hpp"
#include "../../../Algorithm/MeshSample.hpp"
#include "../../../Algorithm/MeshGSOptimize.hpp"
#include "../../../Scene/GMeshView.hpp"
#include <tbb/parallel_for.h>
#include <boost/random/sobol.hpp>
#include <fstream>

namespace GSGI
{

static constexpr uint32_t kSplatPersistVersion = 2;

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
            std::vector<GS3DPackedSplat> meshSplats;

            auto meshSplatPersistPath = pMesh->getPersistPath("GS3D");
            if (!SerializePersist::load(std::ifstream{meshSplatPersistPath}, kSplatPersistVersion, mConfig, meshSplats))
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

                float initialScale = MeshGSOptimize::getInitialScale(sampleResult.totalArea, mConfig.splatsPerMesh);
                logInfo("area: {}, initialScale: {}", sampleResult.totalArea, initialScale);

                auto bvh = MeshClosestPoint::buildBVH(view);
                meshSplats.resize(mConfig.splatsPerMesh);

                tbb::parallel_for(
                    uint32_t{0},
                    mConfig.splatsPerMesh,
                    [&](uint32_t splatID)
                    {
                        meshSplats[splatID] = [&]
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
                                .position = meshPoint.getPosition(view),
                                .rotate = float16_t4(result.rotate.x, result.rotate.y, result.rotate.z, result.rotate.w),
                                .scale = float16_t2(result.scaleXY),
                            };
                        }();
                    }
                );

                SerializePersist::store(std::ofstream{meshSplatPersistPath}, kSplatPersistVersion, mConfig, meshSplats);
            }
            splats.insert(splats.end(), meshSplats.begin(), meshSplats.end());
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