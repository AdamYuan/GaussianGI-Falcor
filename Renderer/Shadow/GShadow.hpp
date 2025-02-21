//
// Created by adamyuan on 12/11/24.
//

#ifndef GSGI_GSHADOW_HPP
#define GSGI_GSHADOW_HPP

#include <Falcor.h>
#include "../../GDeviceObject.hpp"
#include "../../Scene/GStaticScene.hpp"
#include "GShadowType.hpp"

using namespace Falcor;

namespace GSGI
{

class GShadow final : public GDeviceObject<GShadow>
{
private:
    EnumRefTuple<GShadowType> mpShadowTuple;
    float3 mLightDirection{};

public:
    explicit GShadow(const ref<GScene>& pScene);
    ~GShadow() override = default;

    void update(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene);
    void prepareProgram(const ref<Program>& pProgram, const ShaderVar& var, GShadowType type) const;

    void renderUIImpl(Gui::Widgets& widget, const EnumBitset<GShadowType>& activeTypes);
};

} // namespace GSGI

#endif // GSGI_GSHADOW_HPP
