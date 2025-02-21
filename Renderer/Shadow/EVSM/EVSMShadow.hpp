//
// Created by adamyuan on 12/12/24.
//

#ifndef GSGI_EVSMSHADOW_HPP
#define GSGI_EVSMSHADOW_HPP

#include <Falcor.h>
#include "../../../Scene/GSceneObject.hpp"
#include "../../../Scene/GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

class EVSMShadow final : public GSceneObject<EVSMShadow>
{
public:
    explicit EVSMShadow(const ref<GScene>& pScene) : GSceneObject(pScene) {}
    ~EVSMShadow() override = default;

    void updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, bool isLightChanged) {}
    void bindShaderData(const ShaderVar& var) const {}
    void renderUIImpl(Gui::Widgets& widget) {}
};

} // namespace GSGI

#endif // GSGI_EVSMSHADOW_HPP
