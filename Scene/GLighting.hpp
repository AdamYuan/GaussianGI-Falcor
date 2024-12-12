//
// Created by adamyuan on 12/1/24.
//

#pragma once
#ifndef GSGI_GLIGHTING_HPP
#define GSGI_GLIGHTING_HPP

#include <Falcor.h>
#include "../Common/GDeviceObject.hpp"

using namespace Falcor;

namespace GSGI
{

class GLighting final : public GDeviceObject<GLighting>
{
private:
    float3 mColor{1.0f};
    float mIntensity{1.0f};
    float2 mDirectionXZ{0.0f, 0.0f};

public:
    explicit GLighting(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}
    ~GLighting() override = default;

    void bindShaderData(const ShaderVar& var) const;
    void renderUIImpl(Gui::Widgets& w);

    float3 getDirection() const;
    float3 getRadiance() const;
};

} // namespace GSGI

#endif // GSGI_GLIGHTING_HPP
