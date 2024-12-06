//
// Created by adamyuan on 12/6/24.
//

#include "GVBuffer.hpp"

#include "../Common/TextureUtil.hpp"

namespace GSGI
{

GVBuffer::GVBuffer(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    ProgramDesc rasterDesc;
    rasterDesc.addShaderLibrary("GaussianGI/Renderer/GVBuffer.3d.slang").vsEntry("vsMain").gsEntry("gsMain").psEntry("psMain");
    mpRasterPass = RasterPass::create(getDevice(), rasterDesc);
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
        mpIDTexture,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width,
                height,
                ResourceFormat::RG32Uint, //
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
        { return Fbo::create(getDevice(), {mpAlbedoTexture, mpIDTexture}, pScreenFbo->getDepthStencilTexture()); }
    );

    mpRasterPass->getState()->setRasterizerState(pStaticScene->getScene()->getDefaultRasterState());
    pStaticScene->draw(pRenderContext, pScreenFbo, mpRasterPass);
}

} // namespace GSGI