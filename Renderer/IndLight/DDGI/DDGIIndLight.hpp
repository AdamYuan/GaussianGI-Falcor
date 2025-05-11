//
// Created by adamyuan on 12/15/24.
//

#ifndef GSGI_DDGIINDLIGHT_HPP
#define GSGI_DDGIINDLIGHT_HPP

#include <Falcor.h>
#include "../../../GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../../../Scene/GSceneObject.hpp"
#include "../GIndLightArgs.hpp"

using namespace Falcor;

namespace GSGI
{

class DDGIIndLight final : public GSceneObject<DDGIIndLight>
{
private:
    ref<ComputePass> mpProbePass, mpScreenPass;
    ref<Buffer> mpProbeBuffers[2];

    struct
    {
        uint32_t gridDim = 16;
        bool useFoliage = true;
    } mConfig = {};

    struct Grid
    {
        float3 base;
        float unit;
        uint32_t dim;

        void bindShaderData(const ShaderVar& var) const;
        uint32_t getCount() const { return dim * dim * dim; }
    };

    uint mTick{};

public:
    explicit DDGIIndLight(const ref<GScene>& pScene);
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

#endif
