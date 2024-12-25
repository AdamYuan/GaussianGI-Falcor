//
// Created by adamyuan on 12/24/24.
//

#ifndef GSGI_MESHGSOPTIMIZE_HPP
#define GSGI_MESHGSOPTIMIZE_HPP

#include <Falcor.h>
#include <Utils/Math/QuaternionMath.h>
#include "MeshView.hpp"

using namespace Falcor;

namespace GSGI
{

struct MeshGSOptimize
{
    struct Config
    {
        float initialScale;
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

    static Result run(const Concepts::MeshView auto& meshView, const MeshPoint& meshPoint, const Config& cfg)
    {
        float3 rotateZ = PrimitiveViewMethod::getGeomNormal(meshView.getPrimitive(meshPoint.primitiveID));
        auto [rotateX, rotateY] = getRotateXY(rotateZ);

        float3x3 rotateMat;
        rotateMat.setCol(0, rotateX);
        rotateMat.setCol(1, rotateY);
        rotateMat.setCol(2, rotateZ);
        quatf rotateQuat = math::quatFromMatrix(rotateMat);

        return Result{
            .rotate = rotateQuat,
            .scaleXY = float2(cfg.initialScale, cfg.initialScale),
        };
    }
};

} // namespace GSGI

#endif // GSGI_MESHGSOPTIMIZE_HPP
