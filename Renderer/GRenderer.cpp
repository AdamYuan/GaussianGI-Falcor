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
    mpShadow = make_ref<GShadow>(getDevice());
    mpIndirectLight = make_ref<GIndLight>(getDevice());
    mpPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/GRenderer.cs.slang", "csMain");
}

void GRenderer::updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    auto resolution = getTextureResolution2(pTargetFbo);

    // Create Target Texture
    updateTextureSize(
        mpTargetTexture,
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

    // Create Indirect Light Texture
    updateTextureSize(
        mpIndLightTexture,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width,
                height,
                ResourceFormat::RGBA32Float,
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

    mpIndirectLight->update(pRenderContext, isSceneChanged, mpDefaultStaticScene, mConfig.indirectLightType);

    // mpIndirectLight might alter meshes (or sth.) in mpDefaultStaticScene, and we need to use the altered GStaticScene
    ref<GStaticScene> pStaticScene = mpIndirectLight->getStaticScene(mConfig.indirectLightType);

    mpShadow->update(pRenderContext, pStaticScene);

    mpVBuffer->draw(pRenderContext, pTargetFbo, pStaticScene);

    mpIndirectLight->draw(
        pRenderContext,
        {
            .pVBuffer = mpVBuffer,
            .pShadow = mpShadow,
            .shadowType = mConfig.indirectShadowType,
        },
        mpIndLightTexture,
        mConfig.indirectLightType
    );

    {
        auto [prog, var] = getShaderProgVar(mpPass);
        mpVBuffer->bindShaderData(var["gGVBuffer"]);
        mpShadow->prepareProgram(prog, var["gGShadow"], mConfig.directShadowType);
        pStaticScene->bindRootShaderData(var);
        var["gTarget"] = mpTargetTexture;
        var["gIndLight"] = mpIndLightTexture;
        enumVisit(
            mConfig.viewType, [&]<typename EnumInfo_T>(EnumInfo_T) { prog->addDefine("TARGET_VAR_NAME", EnumInfo_T::kProperty.varName); }
        );

        mpPass->execute(pRenderContext, resolution.x, resolution.y);
    }
}

void GRenderer::renderUIImpl(Gui::Widgets& widget)
{
    enumDropdown(widget, "View Type", mConfig.viewType);
    enumDropdown(widget, "Shadow Type (Direct)", mConfig.directShadowType);
    enumDropdown(widget, "Shadow Type (Indirect)", mConfig.indirectShadowType);
    enumDropdown(widget, "Indirect Light Type", mConfig.indirectLightType);
}

} // namespace GSGI