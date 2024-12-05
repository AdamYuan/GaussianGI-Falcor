#pragma once
#include <Falcor.h>
#include <Core/SampleApp.h>
#include <Scene/Camera/CameraController.h>

#include "Scene/GScene.hpp"
#include "Renderer/GRenderer.hpp"

using namespace Falcor;

namespace GSGI
{

class GaussianGI final : public SampleApp
{
public:
    explicit GaussianGI(const SampleAppConfig& config);
    ~GaussianGI() override = default;

    void onLoad(RenderContext* pRenderContext) override;
    void onShutdown() override;
    void onResize(uint32_t width, uint32_t height) override;
    void onFrameRender(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo) override;
    void onGuiRender(Gui* pGui) override;
    bool onKeyEvent(const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(const MouseEvent& mouseEvent) override;
    void onHotReload(HotReloadFlags reloaded) override;

private:
    ref<GScene> mpScene;
    ref<GRenderer> mpRenderer;
    std::unique_ptr<CameraController> mpCameraController;
};

} // namespace GSGI
