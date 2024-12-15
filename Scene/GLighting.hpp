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
    float3 skyRadiance;
    bool operator==(const GLightingData& r) const
    {
        return math::all(radiance == r.radiance) && math::all(direction == r.direction) && math::all(skyRadiance == r.skyRadiance);
    }
};

class GLighting final : public GDeviceObject<GLighting>
{
private:
    float3 mColor{1.0f};
    float mIntensity{4.0f};
    float3 mSkyColor{1.0f};
    float mSkyIntensity{0.1f};
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

} // namespace GSGI

#endif // GSGI_GLIGHTING_HPP
