//
// Created by adamyuan on 12/23/24.
//

#include "GS3DMiscRenderer.hpp"

#include "GS3D.hpp"
#include "../../../Util/ShaderUtil.hpp"

namespace GSGI
{

GS3DMiscRenderer::GS3DMiscRenderer(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    mpVao = Vao::create(Vao::Topology::PointList);
    mpPointPass = RasterPass::create(getDevice(), "GaussianGI/Renderer/IndLight/3DGS/GS3DMiscPoint.3d.slang", "vsMain", "psMain");
    mpPointPass->getState()->setVao(mpVao);
}

void GS3DMiscRenderer::draw(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args)
{
    pRenderContext->clearDsv(pTargetFbo->getDepthStencilView().get(), 1.0f, 0, true, false);
    pRenderContext->clearTexture(pTargetFbo->getColorTexture(0).get(), float4{});

    auto [prog, var] = getShaderProgVar(mpPointPass);
    args.pStaticScene->bindRootShaderData(var);
    var["gSplats"] = args.pSplatBuffer;
    var["gSplatsPerMesh"] = args.splatsPerMesh;

    mpPointPass->getState()->setFbo(pTargetFbo);
    pRenderContext->draw(
        mpPointPass->getState().get(), mpPointPass->getVars().get(), args.splatsPerMesh * args.pStaticScene->getInstanceCount(), 0
    );
}

} // namespace GSGI