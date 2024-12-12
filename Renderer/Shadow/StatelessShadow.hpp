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
    explicit StatelessShadow(const ref<Device>&) {}
    ~StatelessShadow() override = default;

    void update(bool sceneOrLightChanged, const ref<GStaticScene>& pStaticScene) {}
    void bindShaderData(const ShaderVar& var) const {}
};

} // namespace GSGI

#endif // GSGI_STATELESSSHADOW_HPP
