//
// Created by adamyuan on 2/19/25.
//

#include "GS3DIndLightSplat.hpp"

#include <Utils/Math/FormatConversion.h>

namespace GSGI
{

namespace
{
uint32_t packUnorm3x10(const float3& v)
{
    auto x = (uint32_t)math::clamp(v.x * 1023.f + 0.5f, 0.0f, 1023.f);
    auto y = (uint32_t)math::clamp(v.y * 1023.f + 0.5f, 0.0f, 1023.f);
    auto z = (uint32_t)math::clamp(v.z * 1023.f + 0.5f, 0.0f, 1023.f);
    return x | y << 10u | z << 20u;
}
} // namespace

GS3DIndLightPackedSplatGeom GS3DIndLightPackedSplatGeom::fromSplat(const GS3DIndLightSplat& splat)
{
    static_assert(math::all(GMesh::kNormalizedBoundMin == float3{-1.0f}));
    static_assert(math::all(GMesh::kNormalizedBoundMax == float3{1.0f}));

    float4 splatRot = float4{splat.rotate.x, splat.rotate.y, splat.rotate.z, splat.rotate.w};
    splatRot = math::normalize(splatRot);
    if (splatRot.w < 0)
        splatRot = -splatRot;

    GS3DIndLightPackedSplatGeom geom{};
    geom.rotate = packUnorm3x10(splatRot.xyz() * 0.5f + 0.5f);
    geom.meanXY = packSnorm2x16(splat.mean.xy());
    geom.meanZ = packSnorm16(splat.mean.z);
    geom.scale = splat.scale;
    return geom;
}

GS3DIndLightPackedSplatAttrib GS3DIndLightPackedSplatAttrib::fromSplat(const GS3DIndLightSplat& splat)
{
    return GS3DIndLightPackedSplatAttrib{
        .albedo = packUnorm3x10(splat.albedo),
    };
}

void GS3DIndLightInstancedSplatBuffer::bindShaderData(const ShaderVar& var) const
{
    var["splatGeoms"] = pSplatGeomBuffer;
    var["splatAttribs"] = pSplatAttribBuffer;
    var["splatDescs"] = pSplatDescBuffer;
    var["instanceDescs"] = pInstanceDescBuffer;
    var["splatCount"] = splatCount;
}

} // namespace GSGI