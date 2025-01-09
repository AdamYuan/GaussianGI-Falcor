//
// Created by adamyuan on 1/8/25.
//

#ifndef GSGI_GAUSSIANGITRAIN_HPP
#define GSGI_GAUSSIANGITRAIN_HPP

#include <Falcor.h>
#include <Core/SampleApp.h>
#include <Scene/Camera/Camera.h>
#include <Scene/Camera/CameraController.h>
#include "Scene/GMesh.hpp"
#include "Algorithm/MeshGSTrainer/MeshGSTrainer.hpp"

using namespace Falcor;

namespace GSGI
{

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

private:
    ref<GMesh> mpMesh;
    ref<Camera> mpCamera;
    std::unique_ptr<CameraController> mpCameraController;

    MeshGSTrainer<MeshGSTrainType::kDepth> mTrainer;
    MeshGSTrainResource<MeshGSTrainType::kDepth> mTrainResource;

    DeviceSorter<DeviceSortDispatchType::kIndirect> mSorter;
    DeviceSortResource<DeviceSortDispatchType::kIndirect> mSortResource;
};

} // namespace GSGI

#endif // GSGI_GAUSSIANGITRAIN_HPP