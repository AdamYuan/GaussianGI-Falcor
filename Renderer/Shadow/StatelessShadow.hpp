//
// Created by adamyuan on 12/12/24.
//

#ifndef GSGI_STATELESSSHADOW_HPP
#define GSGI_STATELESSSHADOW_HPP

#include <Falcor.h>
#include "../../Scene/GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

class StatelessShadow final : Object
{
public:
    explicit StatelessShadow(const ref<GScene>&) {}
    ~StatelessShadow() override = default;

    void update(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, bool isLightChanged) {}
    void bindShaderData(const ShaderVar& var) const {}
    static void renderUI(Gui::Widgets&) {}
};

} // namespace GSGI

#endif // GSGI_STATELESSSHADOW_HPP
