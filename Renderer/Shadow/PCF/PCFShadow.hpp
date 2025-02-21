//
// Created by adamyuan on 12/12/24.
//

#ifndef GSGI_PCFSHADOW_HPP
#define GSGI_PCFSHADOW_HPP

#include <Falcor.h>
#include "../../../Scene/GSceneObject.hpp"
#include "../../../Scene/GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

class PCFShadow final : public GSceneObject<PCFShadow>
{
public:
    explicit PCFShadow(const ref<GScene>& pScene) : GSceneObject(pScene) {}
    ~PCFShadow() override = default;

    void updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, bool isLightChanged) {}
    void bindShaderData(const ShaderVar& var) const {}
    void renderUIImpl(Gui::Widgets& widget) {}
};

} // namespace GSGI

#endif // GSGI_PCFSHADOW_HPP
