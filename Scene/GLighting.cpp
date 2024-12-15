//
// Created by adamyuan on 12/1/24.
//

#include "GLighting.hpp"

namespace GSGI
{

void GLighting::beginFrame()
{
    mData = {
        .radiance = mColor * mIntensity,
        .direction = math::normalize(float3(mDirectionXZ[0], 1.0f, mDirectionXZ[1])),
        .skyRadiance = mSkyColor * mSkyIntensity,
    };
}

void GLighting::bindShaderData(const ShaderVar& var) const
{
    var["radiance"] = mData.radiance;
    var["direction"] = mData.direction;
    var["skyRadiance"] = mData.skyRadiance;
}

void GLighting::renderUIImpl(Gui::Widgets& w)
{
    w.rgbColor("Color", mColor);
    w.var("Intensity", mIntensity, 0.0f);
    auto direction = float3(mDirectionXZ[0], 1.0f, mDirectionXZ[1]);
    w.var("Direction", direction);
    mDirectionXZ = float2(direction.x, direction.z);
    w.rgbColor("Sky Color", mSkyColor);
    w.var("Sky Intensity", mSkyIntensity, 0.0f);
}

} // namespace GSGI