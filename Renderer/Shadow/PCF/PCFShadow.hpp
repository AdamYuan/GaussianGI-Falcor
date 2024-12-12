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

class PCFShadow final : GDeviceObject<PCFShadow>
{
public:
    explicit PCFShadow(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}
    ~PCFShadow() override = default;

    void update(bool sceneOrLightChanged, const ref<GStaticScene>& pStaticScene) {}
    void bindShaderData(const ShaderVar& var) const {}
};

} // namespace GSGI

#endif // GSGI_PCFSHADOW_HPP
