//
// Created by adamyuan on 1/8/25.
//

#include "GaussianGITrain.hpp"

#include "Scene/GMeshGSTrainDataset.hpp"
#include "Scene/GMeshGSTrainSplatInit.hpp"
#include "Scene/GMeshLoader.hpp"
#include "Scene/GMeshView.hpp"
#include "Renderer/IndLight/GS3D/GS3DIndLightSplat.hpp"
#include "Util/ShaderUtil.hpp"

FALCOR_EXPORT_D3D12_AGILITY_SDK

namespace GSGI
{

using DrawType = GaussianGITrainDrawType;

struct DrawTypeProperty
{
    const char* fieldName;
};
GSGI_ENUM_REGISTER(DrawType::kAlbedo, void, "Albedo", DrawTypeProperty, .fieldName = "channel.albedo");
// GSGI_ENUM_REGISTER(DrawType::kNormal, void, "Normal", DrawTypeProperty, .fieldName = "channel.normal * 0.5 + 0.5");
GSGI_ENUM_REGISTER(DrawType::kDepth, void, "Depth", DrawTypeProperty, .fieldName = "channel.depth");
GSGI_ENUM_REGISTER(DrawType::kT, void, "T", DrawTypeProperty, .fieldName = "T");

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

    Trainer::ResourceDesc trainResourceDesc = {
        .maxSplatCount = kMaxSplatCount,
        .resolution = uint2{getConfig().windowDesc.width, getConfig().windowDesc.height},
    };

    Trainer::Desc trainDesc = {
        .batchSize = 1,
        .learnRate =
            Trainer::Splat{
                .geom =
                    {
                        .rotate = float4{0.0002f},
                        .mean = float3(0.00008f),
                        .scale = float3(0.0002f),
                    },
                .attrib =
                    {
                        .albedo = float3{0.001f},
                    },
            },
        .refineDesc =
            {
                .growGradThreshold = 1e-6f,
                .splitScaleThreshold = 0.005f,
            },
    };
    mTrainResource = Trainer::Resource::create(getDevice(), trainResourceDesc);
    mTrainer = Trainer(getDevice(), trainDesc);

    auto sortDesc = Trainer::getSortDesc();
    mSortResource = DeviceSortResource<DeviceSortDispatchType::kIndirect>::create(getDevice(), sortDesc, kMaxSplatCount);
    mSorter = DeviceSorter<DeviceSortDispatchType::kIndirect>(getDevice(), sortDesc);

    mpDrawPass = FullScreenPass::create(
        getDevice(),
        "GaussianGI/GaussianGITrain.ps.slang",
        []
        {
            DefineList defList;
            Trainer::addDefine(defList);
            return defList;
        }()
    );
}

void GaussianGITrain::resetTrainer()
{
    GMeshGSTrainSplatInit<Trainer::Trait>{.pMesh = mpMesh}.initialize(getRenderContext(), mTrainResource, mConfig.splatCount, 0.2f);
    mTrainState = mTrainer.resetState(mConfig.splatCount);
    mTrainer.getRefineDesc().splatCountLimit = mConfig.splatCount;
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

    if (mpMesh)
    {
        bool doRefine = mConfig.forceRefine;
        if (mConfig.train)
        {
            doRefine = doRefine ||
                       (mConfig.autoRefine && (mTrainState.iteration == 0 ||
                                               mTrainState.iteration >= mTrainState.refineStats.iteration + mConfig.autoRefineIteration));
        }

        if (doRefine)
        {
            mTrainer.refine(mTrainState, pRenderContext, mTrainResource, mSorter, mSortResource);
            mConfig.forceRefine = false;
        }

        if (mConfig.train)
        {
            mTrainDataset.generate(pRenderContext, mTrainData, mTrainResource.desc.resolution, true);
            mTrainer.iterate(mTrainState, pRenderContext, mTrainResource, mTrainData, mSorter, mSortResource);
        }
        else
        {
            mTrainData.camera = MeshGSTrainCamera::create(*mpCamera);
            mTrainDataset.generate(pRenderContext, mTrainData, mTrainResource.desc.resolution, false);
            mTrainer.inference(mTrainState, pRenderContext, mTrainResource, mTrainData.camera, mSorter, mSortResource);
        }
    }

    {
        FALCOR_PROFILE(pRenderContext, "blit");
        auto [prog, var] = getShaderProgVar(mpDrawPass);
        var["gDrawMesh"] = uint32_t(mConfig.drawMeshData);
        mTrainData.meshRT.bindShaderData(var["gMeshRT"]);
        mTrainResource.splatTex.bindShaderData(var["gSplatRT"]);
        enumVisit(
            mConfig.drawType, [&]<typename EnumInfo_T>(EnumInfo_T) { prog->addDefine("FIELD_NAME", EnumInfo_T::kProperty.fieldName); }
        );
        mpDrawPass->execute(pRenderContext, pTargetFbo);
    }
}

void GaussianGITrain::onGuiRender(Gui* pGui)
{
    Gui::Window w(pGui, "Trainer", {250, 200});

    if (w.button("Load Mesh"))
    {
        if (std::filesystem::path filename; openFileDialog({}, filename) && ((mpMesh = GMeshLoader::load(getDevice(), filename))))
        {
            mTrainDataset.pMesh = mpMesh; // Set dataset source
            resetTrainer();
        }
    }
    if (mpMesh)
    {
        if (w.button("Save Mesh Splats"))
        {
            auto splatData = mTrainResource.splatBuf.getData(0, mConfig.splatCount);
            auto splats = splatData.getElements<Trainer::Splat>(mConfig.splatCount);
            std::vector<GS3DIndLightSplat> indLightSplats(mConfig.splatCount);
            for (uint i = 0; i < mConfig.splatCount; ++i)
            {
                const auto& splat = splats[i];
                indLightSplats[i] = GS3DIndLightSplat{
                    .mean = splat.geom.mean,
                    .rotate = quatf{splat.geom.rotate.xyz(), splat.geom.rotate.w},
                    .scale = splat.geom.scale,
                    .albedo = splat.attrib.albedo,
                };
            }

            GS3DIndLightSplat::persistMesh(mpMesh, indLightSplats);
        }
        if (auto g = w.group("Mesh"))
            mpMesh->renderUI(g);
    }
    if (auto g = w.group("Camera"))
        mpCamera->renderUI(g);

    w.checkbox("Draw Mesh", mConfig.drawMeshData);
    enumDropdown(w, "Draw Type", mConfig.drawType);

    static uint32_t sSplatCount = mConfig.splatCount;
    w.var("Splat Count", sSplatCount, 1u, kMaxSplatCount);
    if (w.button("Apply Splat Count"))
    {
        mConfig.splatCount = sSplatCount;
        resetTrainer();
    }

    w.checkbox("Train", mConfig.train);
    if (w.button("Force-Refine"))
        mConfig.forceRefine = true;
    w.checkbox("Auto-Refine", mConfig.autoRefine);
    if (mConfig.autoRefine)
        w.var("Auto-Refine Iteration", mConfig.autoRefineIteration, 1u);
    w.text(fmt::format("Splat Count: {}", mTrainState.splatCount));
    w.text(fmt::format("Iteration: {}", mTrainState.iteration));
    w.text(fmt::format("Batch: {}", mTrainState.batch));
    w.text(fmt::format("Refine Iter.: {}", mTrainState.refineStats.iteration));
    w.text(fmt::format("Refine Keep Cnt.: {}", mTrainState.refineStats.actualKeepCount));
    w.text(fmt::format("Refine Grow Cnt.: {}", mTrainState.refineStats.actualGrowCount));
    w.text(fmt::format("Refine Prune Cnt.: {}", mTrainState.refineStats.pruneCount));
    w.var("Eye Extent", mTrainDataset.config.eyeExtent, 1.0f);
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
    config.windowDesc.width = 1024;
    config.windowDesc.height = 1024;
    config.deviceDesc.type = Device::Vulkan;
    config.generateShaderDebugInfo = true;
    config.shaderPreciseFloat = false;

    GSGI::GaussianGITrain project(config);
    return project.run();
}

int main(int argc, char** argv)
{
    return catchAndReportAllExceptions([&]() { return runMain(argc, argv); });
}
