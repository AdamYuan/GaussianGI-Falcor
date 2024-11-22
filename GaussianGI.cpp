#include "GaussianGI.h"

FALCOR_EXPORT_D3D12_AGILITY_SDK

uint32_t mSampleGuiWidth = 250;
uint32_t mSampleGuiHeight = 200;
uint32_t mSampleGuiPositionX = 20;
uint32_t mSampleGuiPositionY = 40;

GaussianGI::GaussianGI(const SampleAppConfig& config) : SampleApp(config)
{
    //
}

GaussianGI::~GaussianGI()
{
    //
}

void GaussianGI::onLoad(RenderContext* pRenderContext)
{
    //
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
    const float4 clearColor(0.38f, 0.52f, 0.10f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
}

void GaussianGI::onGuiRender(Gui* pGui)
{
    Gui::Window w(pGui, "Falcor", {250, 200});
    renderGlobalUI(pGui);
    w.text("Hello from GaussianGI");
    if (w.button("Click Here"))
    {
        msgBox("Info", "Now why would you do that?");
    }
}

bool GaussianGI::onKeyEvent(const KeyboardEvent& keyEvent)
{
    return false;
}

bool GaussianGI::onMouseEvent(const MouseEvent& mouseEvent)
{
    return false;
}

void GaussianGI::onHotReload(HotReloadFlags reloaded)
{
    //
}

int runMain(int argc, char** argv)
{
    SampleAppConfig config;
    config.windowDesc.title = "Falcor Project Template";
    config.windowDesc.resizableWindow = true;

    GaussianGI project(config);
    return project.run();
}

int main(int argc, char** argv)
{
    return catchAndReportAllExceptions([&]() { return runMain(argc, argv); });
}
