//
// Created by adamyuan on 2/1/25.
//

#include "Depth.hpp"

#include "../MeshGSTrainerImpl.inl"

namespace GSGI
{

MeshGSTrainDepthTrait::SplatTexture MeshGSTrainDepthTrait::SplatTexture::create(const ref<Device>& pDevice, uint2 resolution)
{
    SplatTexture splatTex = {};
    splatTex.pDepthTTexture =
        createTexture<ResourceFormat::R32Float>(pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget);
    return splatTex;
}
void MeshGSTrainDepthTrait::SplatTexture::clearUavRsMs(RenderContext* pRenderContext) const
{
    pRenderContext->clearTexture(pDepthTTexture.get(), float4{0.0f, 1.0f, 0.0f, 0.0f});
}
void MeshGSTrainDepthTrait::SplatTexture::bindShaderData(const ShaderVar& var) const
{
    var["depths_Ts"] = pDepthTTexture;
}
void MeshGSTrainDepthTrait::SplatTexture::bindRsMsShaderData(const ShaderVar& var) const
{
    var["gRs_Ms"] = pDepthTTexture;
}
bool MeshGSTrainDepthTrait::SplatTexture::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pDepthTTexture);
}

MeshGSTrainDepthTrait::SplatRTTexture MeshGSTrainDepthTrait::SplatRTTexture::create(const ref<Device>& pDevice, uint2 resolution)
{
    SplatRTTexture splatRT = {};
    splatRT.pDepthTexture =
        createTexture<ResourceFormat::R32Float>(pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget);
    splatRT.pTTexture =
        createTexture<ResourceFormat::R32Float>(pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget);
    splatRT.pFbo = Fbo::create(pDevice, {splatRT.pDepthTexture, splatRT.pTTexture});
    return splatRT;
}
BlendState::Desc MeshGSTrainDepthTrait::SplatRTTexture::getBlendStateDesc()
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
        .setRenderTargetWriteMask(0, true, false, false, false);
    desc.setRtBlend(1, true)
        .setRtParams(
            1,
            BlendState::BlendOp::Add,
            BlendState::BlendOp::Add, // Ignore
            BlendState::BlendFunc::Zero,
            BlendState::BlendFunc::OneMinusSrcAlpha,
            BlendState::BlendFunc::Zero, // Ignore
            BlendState::BlendFunc::Zero  // Ignore
        )
        .setRenderTargetWriteMask(1, true, false, false, false);
    return desc;
}
void MeshGSTrainDepthTrait::SplatRTTexture::clearRtv(RenderContext* pRenderContext) const
{
    pRenderContext->clearRtv(pDepthTexture->getRTV().get(), float4{});
    pRenderContext->clearRtv(pTTexture->getRTV().get(), float4{1.0f});
}
void MeshGSTrainDepthTrait::SplatRTTexture::bindShaderData(const ShaderVar& var) const
{
    var["depths"] = pDepthTexture;
    var["Ts"] = pTTexture;
}
bool MeshGSTrainDepthTrait::SplatRTTexture::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pDepthTexture, pTTexture, pFbo);
}

MeshGSTrainDepthTrait::MeshRTTexture MeshGSTrainDepthTrait::MeshRTTexture::create(const ref<Device>& pDevice, uint2 resolution)
{
    MeshRTTexture meshRT = {};
    meshRT.pDepthTexture =
        createTexture<ResourceFormat::R32Float>(pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget);
    meshRT.pDepthBuffer = createTexture<ResourceFormat::D32Float>(pDevice, resolution, ResourceBindFlags::DepthStencil);
    meshRT.pFbo = Fbo::create(pDevice, {meshRT.pDepthTexture}, meshRT.pDepthBuffer);
    return meshRT;
}
void MeshGSTrainDepthTrait::MeshRTTexture::clearRtv(RenderContext* pRenderContext) const
{
    pRenderContext->clearRtv(pDepthTexture->getRTV().get(), float4{});
    pRenderContext->clearDsv(pDepthBuffer->getDSV().get(), 1.0f, 0);
}
void MeshGSTrainDepthTrait::MeshRTTexture::bindShaderData(const ShaderVar& var) const
{
    var["depths"] = pDepthTexture;
}
bool MeshGSTrainDepthTrait::MeshRTTexture::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pDepthTexture, pDepthBuffer, pFbo);
}

template struct MeshGSTrainTraitBase<MeshGSTrainDepthTrait>;
template struct MeshGSTrainer<MeshGSTrainDepthTrait>;

} // namespace GSGI