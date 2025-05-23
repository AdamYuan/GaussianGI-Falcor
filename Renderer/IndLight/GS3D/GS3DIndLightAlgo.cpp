//
// Created by adamyuan on 2/8/25.
//

#include "GS3DIndLightAlgo.hpp"

#include "../../../Algorithm/GS3DBound.hpp"
#include "../../../Scene/GMeshView.hpp"
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
constexpr float kSplatBoundFactor = GS3DBound::kSqrt2Log255;
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

bool GS3DIndLightAlgo::SplatTransformData::isTriangleIntersected(float3 v0, float3 v1, float3 v2) const
{
    // From https://iquilezles.org/articles/distfunctions/
    const auto getTriangleSDF = [](float3 p, float3 a, float3 b, float3 c)
    {
        using namespace math;
        float3 ba = b - a;
        float3 pa = p - a;
        float3 cb = c - b;
        float3 pb = p - b;
        float3 ac = a - c;
        float3 pc = p - c;
        float3 nor = cross(ba, ac);
        const auto dot2 = [](auto a) { return dot(a, a); };
        return sqrt(
            (sign(dot(cross(ba, nor), pa)) + sign(dot(cross(cb, nor), pb)) + sign(dot(cross(ac, nor), pc)) < 2.0f)
                ? min(min(dot2(ba * clamp(dot(ba, pa) / dot2(ba), 0.0f, 1.0f) - pa),
                          dot2(cb * clamp(dot(cb, pb) / dot2(cb), 0.0f, 1.0f) - pb)),
                      dot2(ac * clamp(dot(ac, pc) / dot2(ac), 0.0f, 1.0f) - pc))
                : dot(nor, pa) * dot(nor, pa) / dot2(nor)
        );
    };
    v0 = transform(v0);
    v1 = transform(v1);
    v2 = transform(v2);
    return getTriangleSDF(float3{}, v0, v1, v2) < 1.0f;
}

std::vector<GS3DIndLightSplat> GS3DIndLightAlgo::getSplatsFromMeshFallback(
    const GMeshView& meshView,
    const MeshBVH<AABB>& meshBVH,
    uint splatCount
)
{
    std::vector<GS3DIndLightSplat> meshSplats;
    auto sampleResult = MeshSample::sample(
        meshView,
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
                    meshView,
                    meshPoint,
                    meshBVH,
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
                    .mean = meshPoint.getPosition(meshView),
                    .rotate = result.rotate,
                    .scale = float3(result.scaleXY, 0.1f * math::min(result.scaleXY.x, result.scaleXY.y)),
                    .albedo = float3{1.0f},
                };
            }();
        }
    );
    return meshSplats;
}

struct MeshSplatRangeSearcher
{
    GS3DIndLightAlgo::SplatTransformData splatData;
    AABB splatAABB;

    static MeshSplatRangeSearcher fromSplat(const GS3DIndLightSplat& splat)
    {
        MeshSplatRangeSearcher searcher = {.splatData = GS3DIndLightAlgo::SplatTransformData::fromSplat(splat)};
        searcher.splatAABB = searcher.splatData.getAABB();
        return searcher;
    }

    bool isIntersected(AABB bound) const { return bound.intersection(splatAABB).valid(); }
    bool isIntersected(const GMeshPrimitiveView& primitive) const
    {
        auto [v0, v1, v2] = PrimitiveViewMethod::getVertexPositions(primitive);
        return splatData.isTriangleIntersected(v0, v1, v2);
    }
};

std::vector<std::vector<uint32_t>> GS3DIndLightAlgo::getPrimitiveIntersectedSplatIDs(
    const GMeshView& meshView,
    const MeshBVH<AABB>& meshBVH,
    std::span<const GS3DIndLightSplat> splats
)
{
    std::vector<std::vector<uint32_t>> splatIntersectPrimitiveIDs(splats.size());
    tbb::parallel_for(
        uint32_t{0},
        (uint32_t)splats.size(),
        [&](uint32_t splatID)
        {
            splatIntersectPrimitiveIDs[splatID] = [&]
            {
                MeshSplatRangeSearcher searcher = MeshSplatRangeSearcher::fromSplat(splats[splatID]);
                auto result = MeshRangeSearch::query<MeshSplatRangeSearcher>(meshView, meshBVH, searcher);
                return result.primitiveIDs;
            }();
        }
    );

    std::vector<std::vector<uint32_t>> primitiveIntersectedSplatIDs(meshView.getPrimitiveCount());
    for (uint32_t splatID = 0; splatID < splats.size(); ++splatID)
    {
        const auto& primitiveIDs = splatIntersectPrimitiveIDs[splatID];
        for (uint32_t primitiveID : primitiveIDs)
            primitiveIntersectedSplatIDs[primitiveID].push_back(splatID);
    }
    return primitiveIntersectedSplatIDs;
}

} // namespace GSGI