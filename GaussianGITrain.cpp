//
// Created by adamyuan on 1/8/25.
//

#include "GaussianGITrain.hpp"

#include "Algorithm/MeshGSOptimize.hpp"
#include "Algorithm/MeshSample.hpp"
#include "Scene/GMeshLoader.hpp"
#include "Scene/GMeshView.hpp"

#include <boost/random/sobol.hpp>
#include <ranges>

FALCOR_EXPORT_D3D12_AGILITY_SDK

namespace GSGI
{

namespace
{
constexpr uint kMaxSplatCount = 65536;
}

GaussianGITrain::GaussianGITrain(const SampleAppConfig& config) : SampleApp(config)
{
    //
}

void GaussianGITrain::onLoad(RenderContext* pRenderContext)
{
    mpCamera = Camera::create("Main Camera");
    mpCamera->setNearPlane(0.01f);
    mpCamera->setFarPlane(4.0f);
    mpCameraController = std::make_unique<FirstPersonCameraController>(mpCamera);

    MeshGSTrainDesc trainDesc = {
        .maxSplatCount = kMaxSplatCount,
        .resolution = uint2{getConfig().windowDesc.width, getConfig().windowDesc.height},
    };
    mTrainResource = MeshGSTrainResource<MeshGSTrainType::kDepth>::create(getDevice(), trainDesc);
    mTrainer = MeshGSTrainer<MeshGSTrainType::kDepth>(getDevice(), trainDesc);

    auto sortDesc = MeshGSTrainer<MeshGSTrainType::kDepth>::getSortDesc();
    mSortResource = DeviceSortResource<DeviceSortDispatchType::kIndirect>::create(getDevice(), sortDesc, kMaxSplatCount);
    mSorter = DeviceSorter<DeviceSortDispatchType::kIndirect>(getDevice(), sortDesc);
}

void GaussianGITrain::onShutdown()
{
    //
}

void GaussianGITrain::onResize(uint32_t width, uint32_t height)
{
    mpCamera->setAspectRatio(float(width) / float(height));
}

void GaussianGITrain::onFrameRender(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    mpCameraController->update();
    mpCamera->beginFrame();

    mTrainer.forward(pRenderContext, MeshGSTrainCamera::create(*mpCamera), mTrainResource, mSorter, mSortResource);
    pRenderContext->blit(mTrainResource.splatRT.pTextures[0]->getSRV(), pTargetFbo->getColorTexture(0)->getRTV());
}

void GaussianGITrain::onGuiRender(Gui* pGui)
{
    Gui::Window w(pGui, "Trainer", {250, 200});

    if (w.button("Load Mesh"))
    {
        if (std::filesystem::path filename; openFileDialog({}, filename))
            if ((mpMesh = GMeshLoader::load(getDevice(), filename)))
            {
                auto view = GMeshView{mpMesh};
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
                    kMaxSplatCount
                );
                float initialScale = MeshGSOptimize::getInitialScale(sampleResult.totalArea, kMaxSplatCount);
                mTrainResource.splatBuf = MeshGSTrainSplatBuf<MeshGSTrainType::kDepth>::create(
                    getDevice(),
                    kMaxSplatCount,
                    MeshGSTrainSplatBuf<MeshGSTrainType::kDepth>::InitData::create(
                        sampleResult.points | std::views::transform(
                                                  [&](const MeshPoint& meshPoint) -> MeshGSTrainSplat
                                                  {
                                                      auto optimizeResult = MeshGSOptimize::runNoSample(view, meshPoint, initialScale);
                                                      return MeshGSTrainSplat{
                                                          .rotate = optimizeResult.rotate,
                                                          .scale = float3{optimizeResult.scaleXY, 0.1f * initialScale},
                                                          .mean = meshPoint.getPosition(view),
                                                      };
                                                  }
                                              ),
                        kMaxSplatCount
                    )
                );
            }
    }
    if (mpMesh)
    {
        if (auto g = w.group("Mesh"))
            mpMesh->renderUI(g);
    }
}

bool GaussianGITrain::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (mpCameraController->onKeyEvent(keyEvent))
        return true;

    return false;
}

bool GaussianGITrain::onMouseEvent(const MouseEvent& mouseEvent)
{
    if (mpCameraController->onMouseEvent(mouseEvent))
        return true;

    return false;
}

void GaussianGITrain::onHotReload(HotReloadFlags reloaded)
{
    //
}

} // namespace GSGI

int runMain(int argc, char** argv)
{
    SampleAppConfig config;
    config.windowDesc.title = "GaussianGITrain";
    config.windowDesc.resizableWindow = false;
    config.windowDesc.width = 720;
    config.windowDesc.height = 720;
    config.deviceDesc.type = Device::Vulkan;
    config.generateShaderDebugInfo = true;

    GSGI::GaussianGITrain project(config);
    return project.run();
}

int main(int argc, char** argv)
{
    return catchAndReportAllExceptions([&]() { return runMain(argc, argv); });
}
