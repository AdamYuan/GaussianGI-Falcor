//
// Created by adamyuan on 12/6/24.
//

#include "GVBuffer.hpp"

#include "../Util/TextureUtil.hpp"

namespace GSGI
{

GVBuffer::GVBuffer(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    ProgramDesc rasterDesc;
    rasterDesc.addShaderLibrary("GaussianGI/Renderer/GVBuffer.3d.slang").vsEntry("vsMain").gsEntry("gsMain").psEntry("psMain");
    mpRasterPass = RasterPass::create(getDevice(), rasterDesc);
}

uint2 GVBuffer::getResolution() const
{
    return getTextureResolution2(mpAlbedoTexture);
}

void GVBuffer::draw(RenderContext* pRenderContext, const ref<Fbo>& pScreenFbo, const ref<GStaticScene>& pStaticScene)
{
    uint2 resolution = getTextureResolution2(pScreenFbo);
    updateTextureSize(
        mpAlbedoTexture,
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
                ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource
            );
        }
    );
    updateTextureSize(
        mpHitTexture,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width,
                height,
                ResourceFormat::RGBA32Uint, //
                1,
                1,
                nullptr,
                ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource
            );
        }
    );
    updateTextureSize(
        mpFbo,
        resolution,
        [&](uint width, uint height)
        { return Fbo::create(getDevice(), {mpAlbedoTexture, mpHitTexture}, pScreenFbo->getDepthStencilTexture()); }
    );

    mpRasterPass->getState()->setRasterizerState(pStaticScene->getScene()->getDefaultRasterState());

    pRenderContext->clearRtv(mpAlbedoTexture->getRTV().get(), float4{0.0f});
    pRenderContext->clearRtv(mpHitTexture->getRTV().get(), float4{asfloat(0xFFFFFFFFu)});
    pRenderContext->clearDsv(mpFbo->getDepthStencilView().get(), 1.0f, 0, true, false);
    pStaticScene->draw(pRenderContext, mpFbo, mpRasterPass);
}

void GVBuffer::bindShaderData(const ShaderVar& var) const
{
    var["resolution"] = getTextureResolution2(mpAlbedoTexture);
    var["albedoTexture"] = mpAlbedoTexture;
    var["hitTexture"] = mpHitTexture;
}

} // namespace GSGI