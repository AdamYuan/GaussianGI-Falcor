//
// Created by adamyuan on 12/15/24.
//

#ifndef GSGI_PTINDLIGHT_HPP
#define GSGI_PTINDLIGHT_HPP

#include <Falcor.h>
#include "../../../GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../../../Scene/GSceneObject.hpp"
#include "../GIndLightArgs.hpp"

using namespace Falcor;

namespace GSGI
{

class PTIndLight final : public GSceneObject<PTIndLight>
{
private:
    ref<ComputePass> mpPass;

    uint32_t mSPP = 0;

    struct
    {
        uint32_t maxBounce = 5;
    } mConfig = {};

    // Detect changes
    GLightingData mLightingData{};
    float4x4 mViewProjMat{};
    uint2 mResolution{};

public:
    explicit PTIndLight(const ref<GScene>& pScene);
    void updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene);
    void draw(
        RenderContext* pRenderContext,
        const ref<GStaticScene>& pStaticScene,
        const GIndLightDrawArgs& args,
        const ref<Texture>& pIndirectTexture
    );
    static void drawMisc(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, const ref<Fbo>& pTargetFbo)
    {
        pRenderContext->clearTexture(pTargetFbo->getColorTexture(0).get(), float4{});
    }
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_PTINDLIGHT_HPP
