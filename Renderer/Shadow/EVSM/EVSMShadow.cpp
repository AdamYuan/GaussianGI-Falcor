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

    updateTextureSize(mpFbo, resolution, [this](uint width, uint height) { return Fbo::create(getDevice(), {}, mpDepthBuffer); });

    auto [prog, var] = getShaderProgVar(mpDrawPass);
    mTransform.bindShaderData(var["gSMTransform"]);

    // pRenderContext->clearRtv(mpAlbedoTexture->getRTV().get(), float4{0.0f});
    // pRenderContext->clearRtv(mpHitTexture->getRTV().get(), float4{asfloat(0xFFFFFFFFu)});
    pRenderContext->clearDsv(mpFbo->getDepthStencilView().get(), 1.0f, 0, true, false);
    pStaticScene->draw(pRenderContext, mpFbo, mpDrawPass);
}

void EVSMShadow::bindShaderData(const ShaderVar& var) const {}

void EVSMShadow::renderUIImpl(Gui::Widgets& widget)
{
    if (mpDepthBuffer)
        widget.image("Depth", mpDepthBuffer.get());
}

} // namespace GSGI