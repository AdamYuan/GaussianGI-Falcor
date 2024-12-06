//
// Created by adamyuan on 12/5/24.
//

#include "GRenderer.hpp"

namespace GSGI
{

GRenderer::GRenderer(const ref<GScene>& pScene) : GSceneObject(pScene)
{
    mpVBuffer = make_ref<GVBuffer>(getDevice());
}

void GRenderer::updateHasInstanceImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    if (isSceneChanged)
    {
        mpDefaultStaticScene = make_ref<GStaticScene>(getScene());
        logInfo("updateHasInstance {}", getScene()->getVersion());
    }
    mpVBuffer->draw(pRenderContext, pTargetFbo, mpDefaultStaticScene);
    pRenderContext->blit(mpVBuffer->getAlbedoTexture()->getSRV(), pTargetFbo->getColorTexture(0)->getRTV());
}

} // namespace GSGI