//
// Created by adamyuan on 2/3/25.
//

#include "DepthAlbedo.hpp"

#include "../MeshGSTrainerImpl.inl"

namespace GSGI
{

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