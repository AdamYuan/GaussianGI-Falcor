//
// Created by adamyuan on 12/11/24.
//

#ifndef GSGI_GSHADOW_HPP
#define GSGI_GSHADOW_HPP

#include <Falcor.h>
#include "../../Common/GDeviceObject.hpp"
#include "../../Scene/GStaticScene.hpp"
#include "GShadowType.hpp"

using namespace Falcor;

namespace GSGI
{

class GShadow final : public GDeviceObject<GShadow>
{
private:
    EnumRefTuple<GShadowType> mpShadows;
    ref<GStaticScene> mpStaticScene;
    float3 mLightDirection{};

public:
    explicit GShadow(ref<Device> pDevice);
    ~GShadow() override = default;

    void update(const ref<GStaticScene>& pStaticScene);
    void prepareProgram(const ref<Program>& pProgram, const ShaderVar& var, GShadowType type) const;
};

} // namespace GSGI

#endif // GSGI_GSHADOW_HPP
