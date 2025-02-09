//
// Created by adamyuan on 2/8/25.
//

#include "GS3DIndLightAlgo.hpp"

#include "../../../Scene/GMeshView.hpp"
#include "../../../Algorithm/MeshVHBVH.hpp"
#include "../../../Algorithm/MeshSample.hpp"
#include "../../../Algorithm/MeshGSOptimize.hpp"
#include "../../../Algorithm/MeshRangeSearch.hpp"
#include <Utils/Math/QuaternionMath.h>
#include <tbb/parallel_for.h>
#include <boost/random/sobol.hpp>
#include <random>

namespace GSGI
{

namespace
{
constexpr float kSplatBoundFactor = 2.3539888583335364f; // sqrt(log(255))
}

GS3DIndLightAlgo::SplatTransformData GS3DIndLightAlgo::SplatTransformData::fromSplat(const GS3DIndLightSplat& splat)
{
    SplatTransformData data;
    data.mean = splat.mean;
    float3 boundScale = kSplatBoundFactor * float3(splat.scale);
    float3x3 rotateMat = math::matrixFromQuat(quatf{splat.rotate.x, splat.rotate.y, splat.rotate.z, splat.rotate.w});
    data.rotateScaleMat = math::mul(rotateMat, math::matrixFromDiagonal(boundScale));
    data.invRotateScaleMat = math::mul(math::matrixFromDiagonal(1.0f / boundScale), math::transpose(rotateMat));
    return data;
}

float3 GS3DIndLightAlgo::SplatTransformData::rotateScale(const float3& d) const
{
    return math::mul(invRotateScaleMat, d);
}
float3 GS3DIndLightAlgo::SplatTransformData::transform(const float3& p) const
{
    return rotateScale(p - mean);
}
AABB GS3DIndLightAlgo::SplatTransformData::getAABB() const
{
    // See https://members.loria.fr/SHornus/ellipsoid-bbox.html
    float3 extent;
    extent.x = math::length(rotateScaleMat.getRow(0));
    extent.y = math::length(rotateScaleMat.getRow(1));
    extent.z = math::length(rotateScaleMat.getRow(2));
    return AABB{mean - extent, mean + extent};
}

std::vector<GS3DIndLightSplat> GS3DIndLightAlgo::getSplatsFromMeshFallback(const ref<GMesh>& pMesh, uint splatCount)
{
    std::vector<GS3DIndLightSplat> meshSplats;
    auto view = GMeshView{pMesh};
    auto sampleResult = MeshSample::sample(
        view,
        [sobolEngine = boost::random::sobol_engine<uint32_t, 32>{4}] mutable
        {
            uint2 u2;
            float2 f2;
            u2.x = sobolEngine();
            u2.y = sobolEngine();
            f2.x = float(sobolEngine()) / 4294967296.0f;
            f2.y = float(sobolEngine()) / 4294967296.0f;
            return std::tuple{u2, f2};
        },
        splatCount
    );

    float initialScale = MeshGSOptimize::getInitialScale(sampleResult.totalArea, splatCount, 0.5f);
    logInfo("area: {}, initialScale: {}", sampleResult.totalArea, initialScale);

    auto bvh = MeshBVH<AABB>::build<MeshVHBVHBuilder>(view);
    meshSplats.resize(splatCount);

    tbb::parallel_for(
        uint32_t{0},
        splatCount,
        [&](uint32_t splatID)
        {
            meshSplats[splatID] = [&]
            {
                const auto& meshPoint = sampleResult.points[splatID];
                auto result = MeshGSOptimize::run<MeshClosestPointAABBFinder>(
                    view,
                    meshPoint,
                    bvh,
                    MeshGSSamplerDefault{std::mt19937{splatID}},
                    {
                        .initialScale = initialScale,
                        .sampleCount = 128,
                        .epsNormal = 0.2f,
                        .epsDistance = 0.2f * initialScale,
                        .epsScale = 1e-6f,
                        .scaleYMaxIteration = 32,
                    }
                );
                return GS3DIndLightSplat{
                    .mean = meshPoint.getPosition(view),
                    .rotate = float16_t4(result.rotate.x, result.rotate.y, result.rotate.z, result.rotate.w),
                    .scale = float16_t3(result.scaleXY, 0.1f * math::min(result.scaleXY.x, result.scaleXY.y)),
                    .irradiance = float16_t3{1.0f},
                };
            }();
        }
    );
    return meshSplats;
}

struct MeshSplatRangeSearcher
{
    AABB splatAABB;

    bool isIntersected(AABB bound) const { return bound.intersection(splatAABB).valid(); }
    bool isIntersected(const GMeshPrimitiveView& primitive) const {}
};

std::vector<uint32_t> GS3DIndLightAlgo::getSplatIntersectedPrimitiveIDs(const ref<GMesh>& pMesh, const GS3DIndLightSplat& splat) {}

std::vector<std::vector<uint32_t>> GS3DIndLightAlgo::getSplatIntersectedPrimitiveIDs(
    const ref<GMesh>& pMesh,
    std::span<const GS3DIndLightSplat> splats
)
{}

} // namespace GSGI