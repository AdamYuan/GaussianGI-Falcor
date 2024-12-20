#include "GaussianGI.h"

#include "Util/DeviceUtil.hpp"

FALCOR_EXPORT_D3D12_AGILITY_SDK

namespace GSGI
{

GaussianGI::GaussianGI(const SampleAppConfig& config) : SampleApp(config)
{
    //
}

void GaussianGI::onLoad(RenderContext* pRenderContext)
{
    logInfo("WaveGetLaneCount() = {}", deviceWaveGetLaneCount(getDevice()));

    auto camera = Camera::create("Main Camera");
    camera->setNearPlane(0.001f);
    camera->setFarPlane(10.0f);
    mpCameraController = std::make_unique<FirstPersonCameraController>(camera);
    auto lighting = make_ref<GLighting>(getDevice());
    mpScene = make_ref<GScene>(getDevice());
    mpScene->setCamera(camera);
    mpScene->setLighting(lighting);

    mpRenderer = make_ref<GRenderer>(mpScene);
}

void GaussianGI::onShutdown()
{
    //
}

void GaussianGI::onResize(uint32_t width, uint32_t height)
{
    //
}

void GaussianGI::onFrameRender(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    mpCameraController->update();
    mpScene->getCamera()->beginFrame();
    mpScene->getLighting()->beginFrame();
    mpScene->update();

    if (mConfig.enableRender)
    {
        mpRenderer->update(pRenderContext, pTargetFbo);
    }
    else
    {
        pRenderContext->clearFbo(pTargetFbo.get(), float4{0.38f, 0.52f, 0.10f, 1}, 1.0f, 0, FboAttachmentType::All);
        mpScene->draw(pRenderContext, pTargetFbo);
    }
}

void GaussianGI::onGuiRender(Gui* pGui)
{
    Gui::Window w(pGui, "Falcor", {250, 200});

    bool disableSceneEdit = false;

    if (mConfig.enableRender)
    {
        disableSceneEdit = true;
        if (w.button("Stop Renderer"))
            mConfig.enableRender = false;

        if (auto g = w.group("Renderer"))
            mpRenderer->renderUI(g);
    }
    else
    {
        ImGui::BeginDisabled(!mpScene->hasInstance());
        if (w.button("Start Renderer"))
            mConfig.enableRender = true;
        ImGui::EndDisabled();
    }

    if (auto g = w.group("Camera"))
        mpScene->getCamera()->renderUI(g);
    if (auto g = w.group("Lighting"))
        mpScene->getLighting()->renderUI(g);

    if (auto g = w.group("Scene"))
    {
        ImGui::BeginDisabled(disableSceneEdit);
        mpScene->renderUI(g);
        ImGui::EndDisabled();
    }
}

bool GaussianGI::onKeyEvent(const KeyboardEvent& keyEvent)
{
    if (mpCameraController->onKeyEvent(keyEvent))
        return true;

    return false;
}

bool GaussianGI::onMouseEvent(const MouseEvent& mouseEvent)
{
    if (mpCameraController->onMouseEvent(mouseEvent))
        return true;

    return false;
}

void GaussianGI::onHotReload(HotReloadFlags reloaded)
{
    //
}

} // namespace GSGI

int runMain(int argc, char** argv)
{
    SampleAppConfig config;
    config.windowDesc.title = "GaussianGI";
    config.windowDesc.resizableWindow = false;
    config.windowDesc.width = 1280;
    config.windowDesc.height = 720;
    config.deviceDesc.type = Device::Vulkan;
    config.generateShaderDebugInfo = true;

    GSGI::GaussianGI project(config);
    return project.run();
}

int main(int argc, char** argv)
{
    return catchAndReportAllExceptions([&]() { return runMain(argc, argv); });
}
