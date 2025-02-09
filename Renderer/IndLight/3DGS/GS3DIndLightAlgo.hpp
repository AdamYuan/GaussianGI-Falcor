//
// Created by adamyuan on 2/8/25.
//

#ifndef GSGI_GS3DINDLIGHTALGO_HPP
#define GSGI_GS3DINDLIGHTALGO_HPP

#include <Falcor.h>
#include "GS3DIndLightSplat.hpp"

using namespace Falcor;

namespace GSGI
{

struct GS3DIndLightAlgo
{
    struct SplatTransformData
    {
        float3 mean;
        float3x3 rotateScaleMat, invRotateScaleMat;

        static SplatTransformData fromSplat(const GS3DIndLightSplat& splat);

        float3 rotateScale(const float3& d) const;
        float3 transform(const float3& p) const;
        AABB getAABB() const;
        bool isTriangleIntersected(float3 v0, float3 v1, float3 v2) const;
    };

    static std::vector<GS3DIndLightSplat> getSplatsFromMeshFallback(const ref<GMesh>& pMesh, uint splatCount);
    static std::vector<uint32_t> getSplatIntersectedPrimitiveIDs(const ref<GMesh>& pMesh, const GS3DIndLightSplat& splat);
    static std::vector<std::vector<uint32_t>> getSplatIntersectedPrimitiveIDs(
        const ref<GMesh>& pMesh,
        std::span<const GS3DIndLightSplat> splats
    );
};

} // namespace GSGI

#endif // GSGI_GS3DINDLIGHTALGO_HPP
