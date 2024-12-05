//
// Created by adamyuan on 12/5/24.
//

#include "GRenderer.hpp"

namespace GSGI
{

void GRenderer::updateHasInstanceImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    if (isSceneChanged)
    {
        mpDefaultStaticScene = make_ref<GStaticScene>(getScene());
        logInfo("updateHasInstance {}", getScene()->getVersion());
    }
}

} // namespace GSGI