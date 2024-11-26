#include "GaussianGI.h"

#include "Scene/GMeshLoader.hpp"

FALCOR_EXPORT_D3D12_AGILITY_SDK

namespace GSGI
{

GaussianGI::GaussianGI(const SampleAppConfig& config) : SampleApp(config)
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
    w.text("Hello from GaussianGI");
    if (w.button("Click Here"))
    {
        msgBox("Info", "Now why would you do that?");
    }
    if (w.button("Open File"))
    {
        std::filesystem::path path;
        if (openFileDialog({}, path))
        {
            auto optMesh = GMeshLoader::load(path);
            if (optMesh)
            {
                fmt::println("{}", optMesh->name);
                for (const auto& texPath : optMesh->texturePaths)
                {
                    fmt::println("{}", texPath.string());
                }
            }
        }
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

} // namespace GSGI

int runMain(int argc, char** argv)
{
    SampleAppConfig config;
    config.windowDesc.title = "GaussianGI";
    config.windowDesc.resizableWindow = false;
    config.windowDesc.width = 1280;
    config.windowDesc.height = 720;
    config.deviceDesc.type = Device::Vulkan;

    GSGI::GaussianGI project(config);
    return project.run();
}

int main(int argc, char** argv)
{
    return catchAndReportAllExceptions([&]() { return runMain(argc, argv); });
}
