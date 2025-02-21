//
// Created by adamyuan on 12/5/24.
//

#include "GRenderer.hpp"

#include "../Util/TextureUtil.hpp"
#include "../Util/ShaderUtil.hpp"

namespace GSGI
{

GRenderer::GRenderer(const ref<GScene>& pScene) : GSceneObject(pScene)
{
    mpVBuffer = make_ref<GVBuffer>(getDevice());
    mpShadow = make_ref<GShadow>(getScene());
    mpIndirectLight = make_ref<GIndLight>(getScene());
}

void GRenderer::updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    auto resolution = getTextureResolution2(pTargetFbo);

    if (!mpPass)
        mpPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/GRenderer.cs.slang", "csMain");
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
                ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget | ResourceBindFlags::UnorderedAccess
            );
        }
    );

    // Scene-related
    if (!getScene()->hasInstance())
        return;

    if (isSceneChanged)
    {
        mpStaticScene = make_ref<GStaticScene>(getScene(), pRenderContext);
        // logInfo("updateHasInstance {}", getScene()->getVersion());
    }

    mpIndirectLight->update(pRenderContext, mpStaticScene, mConfig.indirectLightType);

    mpShadow->update(pRenderContext, mpStaticScene);

    mpVBuffer->draw(pRenderContext, pTargetFbo, mpStaticScene);

    mpIndirectLight->draw(
        pRenderContext,
        mpStaticScene,
        {
            .pVBuffer = mpVBuffer,
            .pShadow = mpShadow,
            .shadowType = mConfig.indirectShadowType,
        },
        mpIndLightTexture,
        mConfig.indirectLightType
    );

    if (mConfig.drawMisc)
    {
        mpIndirectLight->drawMisc(pRenderContext, mpStaticScene, pTargetFbo, mConfig.indirectLightType);
    }
    else
    {
        auto [prog, var] = getShaderProgVar(mpPass);
        mpVBuffer->bindShaderData(var["gGVBuffer"]);
        mpShadow->prepareProgram(prog, var["gGShadow"], mConfig.directShadowType);
        mpStaticScene->bindRootShaderData(var);
        var["gTarget"] = mpTargetTexture;
        var["gIndLight"] = mpIndLightTexture;
        enumVisit(
            mConfig.viewType, [&]<typename EnumInfo_T>(EnumInfo_T) { prog->addDefine("TARGET_VAR_NAME", EnumInfo_T::kProperty.varName); }
        );

        mpPass->execute(pRenderContext, resolution.x, resolution.y);

        pRenderContext->blit(mpTargetTexture->getSRV(), pTargetFbo->getColorTexture(0)->getRTV());
    }
}

void GRenderer::renderUIImpl(Gui::Widgets& widget)
{
    ImGui::BeginDisabled(mConfig.drawMisc);
    enumDropdown(widget, "View Type", mConfig.viewType);
    ImGui::EndDisabled();
    widget.checkbox("Draw IndLight Misc", mConfig.drawMisc);
    widget.separator();
    enumDropdown(widget, "Shadow Type (Direct)", mConfig.directShadowType);
    enumDropdown(widget, "Shadow Type (Indirect)", mConfig.indirectShadowType);
    if (auto g = widget.group("Shadows", true))
        mpShadow->renderUI(g, enumBitsetMake(mConfig.directShadowType, mConfig.indirectShadowType));
    widget.separator();
    enumDropdown(widget, "Indirect Light Type", mConfig.indirectLightType);
    if (auto g = widget.group("Indirect Lights", true))
        mpIndirectLight->renderUI(g, enumBitsetMake(mConfig.indirectLightType));
}

} // namespace GSGI