//
// Created by adamyuan on 12/12/24.
//

#ifndef GSGI_VSMSHADOW_HPP
#define GSGI_VSMSHADOW_HPP

#include <Falcor.h>
#include "../../../Scene/GSceneObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../../../Algorithm/ShadowMapTransform.hpp"

using namespace Falcor;

namespace GSGI
{

class VSMShadow final : public GSceneObject<VSMShadow>
{
private:
    ShadowMapTransform mTransform;
    ref<RasterPass> mpDrawPass;
    std::array<ref<ComputePass>, 2> mpBlurPasses;
    ref<Texture> mpDepthBuffer;
    std::array<ref<Texture>, 2> mpTextures;
    ref<Fbo> mpFbo;

    struct Config
    {
        static constexpr uint32_t kMinResolution = 128u, kMaxResolution = 4096u, kMaxBlurRadius = 16u;
        uint32_t resolution = 1024u, blurRadius = 2u;
        bool operator==(const Config&) const = default;
    } mConfig = {}, mPrevConfig = {};

    struct
    {
        float vsmBias = 0.01f;
        float bleedReduction = 0.3f;
    } mSampleConfig;

public:
    explicit VSMShadow(const ref<GScene>& pScene);
    ~VSMShadow() override = default;

    void updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, bool isLightChanged);
    void bindShaderData(const ShaderVar& var) const;
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_VSMSHADOW_HPP
