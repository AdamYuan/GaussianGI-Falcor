//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

namespace GSGI
{

GS3DIndLight::GS3DIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}

void GS3DIndLight::update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
{
    if (isActive)
        mpStaticScene = pDefaultStaticScene;
}

void GS3DIndLight::draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture) {}

void GS3DIndLight::drawMisc(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    pRenderContext->clearTexture(pTargetFbo->getColorTexture(0).get(), float4{});
}

void GS3DIndLight::renderUIImpl(Gui::Widgets& widget) {}

} // namespace GSGI