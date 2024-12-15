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

struct GLightingData
{
    float3 radiance;
    float3 direction;
    bool operator==(const GLightingData& r) const { return math::all(radiance == r.radiance) && math::all(direction == r.direction); }
};

class GLighting final : public GDeviceObject<GLighting>
{
public:
    enum class Changes
    {
        kNone = 0b00,
        kRadiance = 0b01,
        kDirection = 0b10,
    };

private:
    float3 mColor{1.0f};
    float mIntensity{1.0f};
    float2 mDirectionXZ{0.0f, 0.0f};

    GLightingData mData{};

public:
    explicit GLighting(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}
    ~GLighting() override = default;

    void bindShaderData(const ShaderVar& var) const;
    void renderUIImpl(Gui::Widgets& w);

    void beginFrame();

    const auto& getData() const { return mData; }
};

FALCOR_ENUM_CLASS_OPERATORS(GLighting::Changes);

} // namespace GSGI

#endif // GSGI_GLIGHTING_HPP
