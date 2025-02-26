//
// Created by adamyuan on 12/12/24.
//

#include "EVSMShadow.hpp"

#include "../../../Util/ShaderUtil.hpp"
#include "../../../Util/TextureUtil.hpp"

namespace GSGI
{

EVSMShadow::EVSMShadow(const ref<GScene>& pScene) : GSceneObject(pScene) {}

void EVSMShadow::updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, bool isLightChanged)
{
    bool doUpdate = isSceneChanged || isLightChanged;

    if (!doUpdate)
        return;

    mTransform = ShadowMapTransform::create(pStaticScene->getBound(), pStaticScene->getLighting()->getData().direction);

    if (!mpDrawPass)
    {
        ProgramDesc rasterDesc;
        rasterDesc.addShaderLibrary("GaussianGI/Renderer/Shadow/EVSM/EVSMDraw.3d.slang")
            .vsEntry("vsMain")
            .gsEntry("gsMain")
            .psEntry("psMain");
        mpDrawPass = RasterPass::create(getDevice(), rasterDesc);
        mpDrawPass->getState()->setRasterizerState(GMesh::getRasterizerState());
    }

    auto resolution = uint2{mConfig.resolution};

    updateTextureSize(
        mpDepthBuffer,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width, height, ResourceFormat::D32Float, 1, 1, nullptr, ResourceBindFlags::DepthStencil | ResourceBindFlags::ShaderResource
            );
        }
    );

    const auto createTexture = [this](uint width, uint height)
    {
        return getDevice()->createTexture2D(
            width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr, ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource
        );
    };
    updateTextureSize(mpTextures[0], resolution, createTexture);
    updateTextureSize(mpTextures[1], resolution, createTexture);

    updateTextureSize(
        mpFbo, resolution, [this](uint width, uint height) { return Fbo::create(getDevice(), {mpTextures[0]}, mpDepthBuffer); }
    );

    auto [prog, var] = getShaderProgVar(mpDrawPass);
    mTransform.bindShaderData(var["gSMTransform"]);

    pRenderContext->clearDsv(mpFbo->getDepthStencilView().get(), 1.0f, 0, true, false);
    pRenderContext->clearRtv(mpFbo->getRenderTargetView(0).get(), float4{0.0f});
    pStaticScene->draw(pRenderContext, mpFbo, mpDrawPass);
}

void EVSMShadow::bindShaderData(const ShaderVar& var) const
{
    var["shadowMap"] = mpTextures[0];
    var["smSampler"] = getDevice()->getDefaultSampler();
    mTransform.bindShaderData(var["smTransform"]);
}

void EVSMShadow::renderUIImpl(Gui::Widgets& widget) {}

} // namespace GSGI