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
private:
    ref<GStaticScene> mpStaticScene;

public:
    explicit NoIndLight(const ref<Device>&) {}
    void update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
    {
        if (isActive)
            mpStaticScene = pDefaultStaticScene;
    }
    const auto& getStaticScene() const { return mpStaticScene; }
    static void draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture)
    {
        pRenderContext->clearTexture(pIndirectTexture.get(), float4{0});
    }
    static void renderUI(Gui::Widgets&) {}
};

} // namespace GSGI

#endif // GSGI_STATELESSINDIRECT_HPP
