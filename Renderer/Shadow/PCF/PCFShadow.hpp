//
// Created by adamyuan on 12/12/24.
//

#ifndef GSGI_PCFSHADOW_HPP
#define GSGI_PCFSHADOW_HPP

#include <Falcor.h>
#include "../../../Common/GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

class PCFShadow final : public GDeviceObject<PCFShadow>
{
public:
    explicit PCFShadow(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}
    ~PCFShadow() override = default;

    void update(RenderContext* pRenderContext, bool isStaticSceneChanged, bool isLightChanged, const ref<GStaticScene>& pStaticScene) {}
    void bindShaderData(const ShaderVar& var) const {}
    void renderUIImpl(Gui::Widgets& widget) {}
};

} // namespace GSGI

#endif // GSGI_PCFSHADOW_HPP
