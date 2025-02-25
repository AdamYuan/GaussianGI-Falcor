//
// Created by adamyuan on 2/25/25.
//

#ifndef GSGI_ALGO_SHADOWMAPTRANSFORM_HPP
#define GSGI_ALGO_SHADOWMAPTRANSFORM_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct ShadowMapTransform
{
    float3x4 lightMat;

    static ShadowMapTransform create(const AABB& bound, const float3& lightDir)
    {
        float3 lightSide = lightDir.x < lightDir.y && lightDir.x < lightDir.z
                               ? float3(1, 0, 0)
                               : (lightDir.y < lightDir.z ? float3(0, 1, 0) : float3(0, 0, 1));
        lightSide = math::normalize(math::cross(lightDir, lightSide));
        float3 lightUp = math::cross(lightSide, lightDir);

        float3x3 absLightDirMatT;
        absLightDirMatT.setRow(0, math::abs(lightSide));
        absLightDirMatT.setRow(1, math::abs(lightUp));
        absLightDirMatT.setRow(2, math::abs(lightDir));

        auto extent = bound.extent();
        auto lightDirHalfExtent = math::mul(absLightDirMatT, extent) * 0.5f;

        float3 center = bound.center();

        float3x4 lightMat;
        lightMat.setRow(0, float4{lightSide / lightDirHalfExtent.x, -center.x});
        lightMat.setRow(1, float4{lightUp / lightDirHalfExtent.y, -center.y});
        lightMat.setRow(2, float4{-lightDir / lightDirHalfExtent.z, -center.z}); // Negative lightDir due to depth

        return ShadowMapTransform{.lightMat = lightMat};
    }

    void bindShaderData(const ShaderVar& var) const { var["lightMat"] = lightMat; }
};

} // namespace GSGI

#endif
