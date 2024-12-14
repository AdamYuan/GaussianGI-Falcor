//
// Created by adamyuan on 12/12/24.
//

#ifndef GSGI_EVSMSHADOW_HPP
#define GSGI_EVSMSHADOW_HPP

#include <Falcor.h>
#include "../../../Common/GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

class EVSMShadow final : GDeviceObject<EVSMShadow>
{
public:
    explicit EVSMShadow(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}
    ~EVSMShadow() override = default;

    void update(RenderContext* pRenderContext, bool isStaticSceneChanged, bool isLightChanged, const ref<GStaticScene>& pStaticScene) {}
    void bindShaderData(const ShaderVar& var) const {}
};

} // namespace GSGI

#endif // GSGI_EVSMSHADOW_HPP
