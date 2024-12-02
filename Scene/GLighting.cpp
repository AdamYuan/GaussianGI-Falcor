//
// Created by adamyuan on 12/1/24.
//

#include "GLighting.hpp"

namespace GSGI
{

void GLighting::bindShaderData(const ShaderVar& var) const
{
    var["radiance"] = mColor * mIntensity;
    var["direction"] = math::normalize(float3(mDirectionXZ[0], 1.0f, mDirectionXZ[1]));
}

void GLighting::renderUIImpl(Gui::Widgets& w)
{
    w.rgbColor("Color", mColor);
    w.var("Intensity", mIntensity, 0.0f);
    auto direction = float3(mDirectionXZ[0], 1.0f, mDirectionXZ[1]);
    w.var("Direction", direction);
    mDirectionXZ = float2(direction.x, direction.z);
}

} // namespace GSGI