//
// Created by adamyuan on 12/6/24.
//

#include "GVBuffer.hpp"

#include "../Util/TextureUtil.hpp"

namespace GSGI
{

GVBuffer::GVBuffer(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}

uint2 GVBuffer::getResolution() const
{
    return getTextureResolution2(mpAlbedoTexture);
}

void GVBuffer::draw(RenderContext* pRenderContext, const ref<Fbo>& pScreenFbo, const ref<GStaticScene>& pStaticScene)
{
    FALCOR_PROFILE(pRenderContext, "GVBuffer::draw");

    if (!mpRasterPass)
    {
        ProgramDesc rasterDesc;
        rasterDesc.addShaderLibrary("GaussianGI/Renderer/GVBuffer.3d.slang").vsEntry("vsMain").gsEntry("gsMain").psEntry("psMain");
        mpRasterPass = RasterPass::create(getDevice(), rasterDesc);

        DepthStencilState::Desc depthStencilDesc;
        depthStencilDesc.setDepthEnabled(true);
        depthStencilDesc.setDepthWriteMask(true);
        depthStencilDesc.setStencilEnabled(true);
        depthStencilDesc.setStencilOp(
            DepthStencilState::Face::FrontAndBack,
            DepthStencilState::StencilOp::Replace,
            DepthStencilState::StencilOp::Replace,
            DepthStencilState::StencilOp::Replace
        );
        depthStencilDesc.setStencilFunc(DepthStencilState::Face::FrontAndBack, ComparisonFunc::Always);
        depthStencilDesc.setStencilWriteMask(0xFF);
        depthStencilDesc.setStencilReadMask(0xFF);
        depthStencilDesc.setStencilRef(1);
        mpRasterPass->getState()->setDepthStencilState(DepthStencilState::create(depthStencilDesc));
    }
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
        mpDepthStencilTexture,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width,
                height,
                ResourceFormat::D32FloatS8Uint, //
                1,
                1,
                nullptr,
                ResourceBindFlags::DepthStencil
            );
        }
    );
    updateTextureSize(
        mpFbo,
        resolution,
        [&](uint width, uint height) { return Fbo::create(getDevice(), {mpAlbedoTexture, mpHitTexture}, mpDepthStencilTexture); }
    );

    mpRasterPass->getState()->setRasterizerState(GMesh::getRasterizerState());

    pRenderContext->clearRtv(mpAlbedoTexture->getRTV().get(), float4{0.0f});
    pRenderContext->clearRtv(mpHitTexture->getRTV().get(), float4{asfloat(0xFFFFFFFFu)});
    pRenderContext->clearDsv(mpFbo->getDepthStencilView().get(), 1.0f, 0, true, true);
    pStaticScene->draw(pRenderContext, mpFbo, mpRasterPass);
}

void GVBuffer::bindShaderData(const ShaderVar& var) const
{
    var["resolution"] = getTextureResolution2(mpAlbedoTexture);
    var["albedoTexture"] = mpAlbedoTexture;
    var["hitTexture"] = mpHitTexture;
}

} // namespace GSGI