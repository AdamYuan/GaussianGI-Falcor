//
// Created by adamyuan on 2/1/25.
//

#include "Depth.hpp"

#include "../MeshGSTrainerImpl.inl"

namespace GSGI
{

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

template struct MeshGSTrainer<MeshGSTrainDepthTrait>;

} // namespace GSGI