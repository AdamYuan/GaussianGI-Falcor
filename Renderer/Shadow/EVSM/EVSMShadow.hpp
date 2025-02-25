//
// Created by adamyuan on 12/12/24.
//

#ifndef GSGI_EVSMSHADOW_HPP
#define GSGI_EVSMSHADOW_HPP

#include <Falcor.h>
#include "../../../Scene/GSceneObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../../../Algorithm/ShadowMapTransform.hpp"

using namespace Falcor;

namespace GSGI
{

class EVSMShadow final : public GSceneObject<EVSMShadow>
{
private:
    ShadowMapTransform mTransform;
    ref<RasterPass> mpDrawPass;
    ref<Texture> mpDepthBuffer;
    ref<Fbo> mpFbo;

    struct
    {
        uint32_t resolution = 1024u;
    } mConfig = {};

public:
    explicit EVSMShadow(const ref<GScene>& pScene);
    ~EVSMShadow() override = default;

    void updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, bool isLightChanged);
    void bindShaderData(const ShaderVar& var) const;
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_EVSMSHADOW_HPP
