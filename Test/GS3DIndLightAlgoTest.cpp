//
// Created by adamyuan on 2/9/25.
//

#include "Falcor.h"
#include "Testing/UnitTest.h"
#include <Utils/Math/QuaternionMath.h>
#include <random>

#include "../Renderer/IndLight/3DGS/GS3DIndLightAlgo.hpp"

using namespace Falcor;

namespace GSGI
{

CPU_TEST(GS3DIndLightAlgo_SplatAABBTest)
{
    float3x3 rotateMat = math::matrixFromLookAt(float3(1, 1, 1), float3(0, 0, 0), float3(0, 0, 1));
    quatf rotateQuat = math::quatFromMatrix(rotateMat);
    rotateQuat = math::normalize(rotateQuat);

    fmt::println("{}", rotateMat);
    fmt::println("{}", rotateQuat);

    GS3DIndLightSplat splat{};
    splat.mean = float3{};
    splat.scale = float3{1.0f, 3.0f, 5.0f};
    splat.rotate = float4{rotateQuat.x, rotateQuat.y, rotateQuat.z, rotateQuat.w};

    auto data = GS3DIndLightAlgo::SplatTransformData::fromSplat(splat);
    auto aabb = data.getAABB();

    AABB sampleAABB{splat.mean};

    std::mt19937 randGen{};
    std::uniform_real_distribution<float> distr{0.0f, 1.0f};
    for (uint32_t i = 0; i < 10000; ++i)
    {
        float theta = 2.0f * float(M_PI) * distr(randGen);
        float phi = math::acos(2.0f * distr(randGen) - 1.0f);
        float3 p = {math::sin(phi) * math::cos(theta), math::sin(phi) * math::sin(theta), math::cos(phi)};
        float3 splatP = mul(rotateMat, float3{splat.scale} * p);
        sampleAABB.include(splatP);
    }

    fmt::println("{}, {}", aabb.minPoint, aabb.maxPoint);
    fmt::println("{}, {}", sampleAABB.minPoint, sampleAABB.maxPoint);
    fmt::println("{}", aabb.minPoint / sampleAABB.minPoint);
}

} // namespace GSGI
