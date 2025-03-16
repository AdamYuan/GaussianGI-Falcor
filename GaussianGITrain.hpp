//
// Created by adamyuan on 1/8/25.
//

#ifndef GSGI_GAUSSIANGITRAIN_HPP
#define GSGI_GAUSSIANGITRAIN_HPP

#include <Falcor.h>
#include <Core/SampleApp.h>
#include <Scene/Camera/Camera.h>
#include <Scene/Camera/CameraController.h>
#include <Core/Pass/FullScreenPass.h>
#include "Scene/GMesh.hpp"
#include "Algorithm/MeshGSTrainer/Trait/DepthAlbedo.hpp"
#include "Scene/GMeshGSTrainDataset.hpp"
#include "Util/EnumUtil.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GaussianGITrainDrawType
{
    kAlbedo,
    // kNormal,
    kDepth,
    kT,
    GSGI_ENUM_COUNT
};

class GaussianGITrain final : public SampleApp
{
public:
    explicit GaussianGITrain(const SampleAppConfig& config);
    ~GaussianGITrain() override = default;

    void onLoad(RenderContext* pRenderContext) override;
    void onShutdown() override;
    void onResize(uint32_t width, uint32_t height) override;
    void onFrameRender(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo) override;
    void onGuiRender(Gui* pGui) override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onHotReload(HotReloadFlags reloaded) override;

    void resetTrainer();

private:
    using Trainer = MeshGSTrainer<MeshGSTrainDepthAlbedoTrait>;

    static constexpr uint32_t kMaxSplatCount = 65536;
    struct
    {
        bool drawMeshData = false;
        bool train = false;
        bool refine = false;
        uint32_t splatCount = kMaxSplatCount;
        GaussianGITrainDrawType drawType = GaussianGITrainDrawType::kAlbedo;
    } mConfig = {};
    ref<GMesh> mpMesh;
    ref<Camera> mpCamera;
    std::unique_ptr<CameraController> mpCameraController;

    ref<FullScreenPass> mpDrawPass;

    Trainer mTrainer;
    Trainer::Resource mTrainResource;
    Trainer::Data mTrainData{};
    GMeshGSTrainDataset<Trainer::Trait> mTrainDataset;
    MeshGSTrainState mTrainState{};

    DeviceSorter<DeviceSortDispatchType::kIndirect> mSorter;
    DeviceSortResource<DeviceSortDispatchType::kIndirect> mSortResource;
};

} // namespace GSGI

#endif // GSGI_GAUSSIANGITRAIN_HPP
