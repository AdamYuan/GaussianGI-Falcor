//
// Created by adamyuan on 12/14/24.
//

#ifndef GSGI_STATELESSINDIRECT_HPP
#define GSGI_STATELESSINDIRECT_HPP

#include <Falcor.h>
#include "../../Scene/GStaticScene.hpp"
#include "GIndLightArgs.hpp"

using namespace Falcor;

namespace GSGI
{

class NoIndLight final : public Object
{
public:
    explicit NoIndLight(const ref<GScene>&) {}
    void update(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene) {}
    static void draw(
        RenderContext* pRenderContext,
        const ref<GStaticScene>& pStaticScene,
        const GIndLightDrawArgs& args,
        const ref<Texture>& pIndirectTexture
    )
    {
        pRenderContext->clearTexture(pIndirectTexture.get(), float4{0});
    }
    static void drawMisc(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, const ref<Fbo>& pTargetFbo)
    {
        pRenderContext->clearTexture(pTargetFbo->getColorTexture(0).get(), float4{});
    }
    static void renderUI(Gui::Widgets&) {}
};

} // namespace GSGI

#endif // GSGI_STATELESSINDIRECT_HPP
