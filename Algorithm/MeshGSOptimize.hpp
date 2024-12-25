//
// Created by adamyuan on 12/24/24.
//

#ifndef GSGI_MESHGSOPTIMIZE_HPP
#define GSGI_MESHGSOPTIMIZE_HPP

#include <Falcor.h>
#include <Utils/Math/QuaternionMath.h>
#include "MeshClosestPoint.hpp"
#include "MeshView.hpp"

using namespace Falcor;

namespace GSGI
{

namespace Concepts
{
template<typename T>
concept MeshGS2DSampler = requires(T& t, float2 sigma) {
    { t(sigma) } -> std::convertible_to<float2>;
};
template<typename T>
concept MeshGS3DSampler = requires(T& t, float3 sigma) {
    { t(sigma) } -> std::convertible_to<float3>;
};
} // namespace Concepts

template<typename Rand_T>
struct MeshGSSamplerDefault
{
    Rand_T rand;

    float2 operator()(const float2& sigma2)
    {
        float2 f2;
        f2.x = std::normal_distribution<float>{0.0f, sigma2.x}(rand);
        f2.y = std::normal_distribution<float>{0.0f, sigma2.y}(rand);
        return f2;
    }
    float3 operator()(const float3& sigma3)
    {
        float3 f3;
        f3.x = std::normal_distribution<float>{0.0f, sigma3.x}(rand);
        f3.y = std::normal_distribution<float>{0.0f, sigma3.y}(rand);
        f3.z = std::normal_distribution<float>{0.0f, sigma3.z}(rand);
        return f3;
    }
};

static_assert(Concepts::MeshGS2DSampler<MeshGSSamplerDefault<std::mt19937>>);
static_assert(Concepts::MeshGS3DSampler<MeshGSSamplerDefault<std::mt19937>>);

struct MeshGSOptimize
{
    struct Config
    {
        float initialScale;
        uint32_t sampleCount;
        float epsNormal, epsDistance, epsScale;
        uint32_t scaleYMaxIteration;
    };
    struct Result
    {
        quatf rotate;
        float2 scaleXY;
    };

    static std::tuple<float3, float3> getRotateXY(const float3& rotateZ)
    {
        float3 rotateX = math::normalize(math::cross(math::abs(rotateZ.x) > .01 ? float3(0, 1, 0) : float3(1, 0, 0), rotateZ));
        float3 rotateY = math::cross(rotateZ, rotateX);
        return {rotateX, rotateY};
    }

    static Result run(
        const Concepts::MeshView auto& meshView,
        const MeshPoint& meshPoint,
        const MeshClosestPointBVH& meshBvh,
        Concepts::MeshGS2DSampler auto& sampler,
        const Config& cfg
    )
    {
        float3 normal = PrimitiveViewMethod::getGeomNormal(meshPoint.getPrimitive(meshView));
        float3 center = meshPoint.getPosition(meshView);

        float3 rotateZ = normal;
        auto [rotateX, rotateY] = getRotateXY(rotateZ);

        float2 scaleXY = {cfg.initialScale, cfg.initialScale};

        // Find outliers
        std::vector<float3> outliers;
        for (uint32_t i = 0; i < cfg.sampleCount; ++i)
        {
            float3 sample = [&]
            {
                float2 f2 = sampler(float2(cfg.initialScale));
                return rotateX * f2.x + rotateY * f2.y;
            }();

            bool isOutlier = [&]
            {
                float maxDist2 = cfg.epsDistance * cfg.epsDistance;
                auto closestPointResult = MeshClosestPoint::query(meshView, meshBvh, center + sample, maxDist2);
                if (!closestPointResult.optPrimitiveID)
                {
                    // distance >= cfg.epsDistance, outlier
                    return true;
                }
                uint32_t samplePrimitiveID = *closestPointResult.optPrimitiveID;
                if (samplePrimitiveID == meshPoint.primitiveID)
                {
                    // On the same primitive, not outlier
                    return false;
                }
                float3 sampleNormal = PrimitiveViewMethod::getGeomNormal(meshView.getPrimitive(samplePrimitiveID));
                return math::dot(sampleNormal, normal) < 1.0f - cfg.epsNormal;
            }();

            if (isOutlier)
                outliers.push_back(sample);
        }

        if (!outliers.empty())
        {
            // Find closest dissimilar sample (outlier)
            {
                float3 closestOutlier;
                float closestOutlierDist2 = std::numeric_limits<float>::infinity();

                for (const float3& outlier : outliers)
                {
                    float outlierDist2 = math::dot(outlier, outlier);
                    if (outlierDist2 < closestOutlierDist2)
                    {
                        closestOutlier = outlier;
                        closestOutlierDist2 = outlierDist2;
                    }
                }

                scaleXY.x = math::sqrt(closestOutlierDist2);
                rotateX = math::normalize(closestOutlier);
                rotateY = math::cross(rotateZ, rotateX);
            }

            // Find scaleY
            {
                math::matrix<float, 2, 3> invRotateXYMat;
                invRotateXYMat.setRow(0, rotateX);
                invRotateXYMat.setRow(1, rotateY);
                float minScaleY = scaleXY.x, maxScaleY = cfg.initialScale;
                for (uint32_t i = 0; i < cfg.scaleYMaxIteration; ++i)
                {
                    float scaleYDiff = maxScaleY - minScaleY;
                    if (scaleYDiff <= cfg.epsScale)
                        break;
                    float midScaleY = minScaleY + scaleYDiff * 0.5f;
                    float2 invScaleXY = 1.0f / float2(scaleXY.x, midScaleY);

                    bool isOutlierCovered = false;
                    for (const float3& outlier : outliers)
                    {
                        float2 circlePos = math::mul(invRotateXYMat, outlier) * invScaleXY;
                        if (math::dot(circlePos, circlePos) < 1.0f)
                        {
                            isOutlierCovered = true;
                            break;
                        }
                    }

                    if (isOutlierCovered)
                        maxScaleY = midScaleY;
                    else
                        minScaleY = midScaleY;
                }
                scaleXY.y = maxScaleY;
            }
        }

        return Result{
            .rotate =
                [rotateX, rotateY, rotateZ]()
            {
                float3x3 rotateMat;
                rotateMat.setCol(0, rotateX);
                rotateMat.setCol(1, rotateY);
                rotateMat.setCol(2, rotateZ);
                return math::quatFromMatrix(rotateMat);
            }(),
            .scaleXY = scaleXY,
        };
    }
};

} // namespace GSGI

#endif // GSGI_MESHGSOPTIMIZE_HPP
