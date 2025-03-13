//
// Created by adamyuan on 2/3/25.
//

#include "DepthAlbedoNormal.hpp"

#include "../MeshGSTrainerImpl.inl"

namespace GSGI
{

MeshGSTrainDepthAlbedoNormalTrait::MeshRTTexture MeshGSTrainDepthAlbedoNormalTrait::MeshRTTexture::create(
    const ref<Device>& pDevice,
    uint2 resolution
)
{
    MeshRTTexture meshRT = {};
    meshRT.pTexture = createTexture<ResourceFormat::RGBA32Float>(
        pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget
    );
    meshRT.pDepthBuffer = createTexture<ResourceFormat::D32Float>(pDevice, resolution, ResourceBindFlags::DepthStencil);
    meshRT.pFbo = Fbo::create(pDevice, {meshRT.pTexture}, meshRT.pDepthBuffer);
    return meshRT;
}
void MeshGSTrainDepthAlbedoNormalTrait::MeshRTTexture::clearRtv(RenderContext* pRenderContext) const
{
    pRenderContext->clearRtv(pTexture->getRTV().get(), float4{});
    pRenderContext->clearDsv(pDepthBuffer->getDSV().get(), 1.0f, 0);
}
void MeshGSTrainDepthAlbedoNormalTrait::MeshRTTexture::bindShaderData(const ShaderVar& var) const
{
    var["tex"] = pTexture;
}
bool MeshGSTrainDepthAlbedoNormalTrait::MeshRTTexture::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pTexture, pDepthBuffer, pFbo);
}

template class MeshGSTrainer<MeshGSTrainDepthAlbedoNormalTrait>;

} // namespace GSGI