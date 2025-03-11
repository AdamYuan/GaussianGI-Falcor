//
// Created by adamyuan on 2/3/25.
//

#include "DepthAlbedo.hpp"

#include "../MeshGSTrainerImpl.inl"

namespace GSGI
{

MeshGSTrainDepthAlbedoTrait::SplatRTTexture MeshGSTrainDepthAlbedoTrait::SplatRTTexture::create(
    const ref<Device>& pDevice,
    uint2 resolution
)
{
    SplatRTTexture splatRT = {};
    splatRT.pAlbedoOneMinusTTexture = createTexture<ResourceFormat::RGBA32Float>(
        pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget
    );
    splatRT.pDepthTexture =
        createTexture<ResourceFormat::R32Float>(pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget);
    splatRT.pFbo = Fbo::create(pDevice, {splatRT.pAlbedoOneMinusTTexture, splatRT.pDepthTexture});
    return splatRT;
}
BlendState::Desc MeshGSTrainDepthAlbedoTrait::SplatRTTexture::getBlendStateDesc()
{
    BlendState::Desc desc;
    desc.setIndependentBlend(true);
    desc.setRtBlend(0, true)
        .setRtParams(
            0,
            BlendState::BlendOp::Add,
            BlendState::BlendOp::Add,
            BlendState::BlendFunc::SrcAlpha,
            BlendState::BlendFunc::OneMinusSrcAlpha,
            BlendState::BlendFunc::One,
            BlendState::BlendFunc::OneMinusSrcAlpha
        )
        .setRenderTargetWriteMask(0, true, true, true, true);
    desc.setRtBlend(1, true)
        .setRtParams(
            1,
            BlendState::BlendOp::Add,
            BlendState::BlendOp::Add,
            BlendState::BlendFunc::SrcAlpha,
            BlendState::BlendFunc::OneMinusSrcAlpha,
            BlendState::BlendFunc::One,
            BlendState::BlendFunc::OneMinusSrcAlpha
        )
        .setRenderTargetWriteMask(1, true, false, false, false);
    return desc;
}
void MeshGSTrainDepthAlbedoTrait::SplatRTTexture::clearRtv(RenderContext* pRenderContext) const
{
    pRenderContext->clearRtv(pAlbedoOneMinusTTexture->getRTV().get(), float4{});
    pRenderContext->clearRtv(pDepthTexture->getRTV().get(), float4{});
}
void MeshGSTrainDepthAlbedoTrait::SplatRTTexture::bindShaderData(const ShaderVar& var) const
{
    var["albedos_oneMinusTs"] = pAlbedoOneMinusTTexture;
    var["depths"] = pDepthTexture;
}
bool MeshGSTrainDepthAlbedoTrait::SplatRTTexture::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pAlbedoOneMinusTTexture, pDepthTexture, pFbo);
}

MeshGSTrainDepthAlbedoTrait::MeshRTTexture MeshGSTrainDepthAlbedoTrait::MeshRTTexture::create(const ref<Device>& pDevice, uint2 resolution)
{
    MeshRTTexture meshRT = {};
    meshRT.pAlbedoDepthTexture = createTexture<ResourceFormat::RGBA32Float>(
        pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget
    );
    meshRT.pDepthBuffer = createTexture<ResourceFormat::D32Float>(pDevice, resolution, ResourceBindFlags::DepthStencil);
    meshRT.pFbo = Fbo::create(pDevice, {meshRT.pAlbedoDepthTexture}, meshRT.pDepthBuffer);
    return meshRT;
}
void MeshGSTrainDepthAlbedoTrait::MeshRTTexture::clearRtv(RenderContext* pRenderContext) const
{
    pRenderContext->clearRtv(pAlbedoDepthTexture->getRTV().get(), float4{});
    pRenderContext->clearDsv(pDepthBuffer->getDSV().get(), 1.0f, 0);
}
void MeshGSTrainDepthAlbedoTrait::MeshRTTexture::bindShaderData(const ShaderVar& var) const
{
    var["albedos_depths"] = pAlbedoDepthTexture;
}
bool MeshGSTrainDepthAlbedoTrait::MeshRTTexture::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pAlbedoDepthTexture, pDepthBuffer, pFbo);
}

template struct MeshGSTrainer<MeshGSTrainDepthAlbedoTrait>;

} // namespace GSGI