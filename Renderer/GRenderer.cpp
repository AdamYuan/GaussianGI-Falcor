//
// Created by adamyuan on 12/5/24.
//

#include "GRenderer.hpp"

#include "../Common/TextureUtil.hpp"
#include "../Common/ShaderUtil.hpp"

namespace GSGI
{

GRenderer::GRenderer(const ref<GScene>& pScene) : GSceneObject(pScene)
{
    mpVBuffer = make_ref<GVBuffer>(getDevice());
    mpPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/GRenderer.cs.slang", "csMain");
}

void GRenderer::updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    auto resolution = getTextureResolution2(pTargetFbo);
    // Create Texture
    updateTextureSize(
        mpTexture,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width,
                height,
                ResourceFormat::RGBA8Unorm,
                1,
                1,
                nullptr,
                ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
            );
        }
    );

    // Scene-related
    if (!getScene()->hasInstance())
        return;

    if (isSceneChanged)
    {
        mpDefaultStaticScene = make_ref<GStaticScene>(getScene(), pRenderContext);
        logInfo("updateHasInstance {}", getScene()->getVersion());
    }

    mpVBuffer->draw(pRenderContext, pTargetFbo, mpDefaultStaticScene);

    {
        auto [prog, var] = getShaderProgVar(mpPass);
        mpVBuffer->bindShaderData(var["gGVBuffer"]);
        mpDefaultStaticScene->bindRootShaderData(var);
        var["gTarget"] = mpTexture;

        mpPass->execute(pRenderContext, resolution.x, resolution.y);
    }
}

} // namespace GSGI