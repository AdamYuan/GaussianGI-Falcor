#pragma once

#include <Falcor.h>
#include <array>

using namespace Falcor;

namespace GSGI
{

struct Octahedron
{
    static constexpr uint kVertexCount = 6;
    static constexpr std::array<float3, kVertexCount> kVertices = {
        float3(-1, 0, 0),
        float3(1, 0, 0),
        float3(0, -1, 0),
        float3(0, 1, 0),
        float3(0, 0, -1),
        float3(0, 0, 1),
    };

    static constexpr float kFaceDist = 0.5773502691896257; // sqrt(3) / 3

    static constexpr uint kTriangleCount = 8;
    static constexpr std::array<uint3, kTriangleCount> kTriangles = {
        uint3(3, 1, 5),
        uint3(3, 5, 0),
        uint3(3, 0, 4),
        uint3(3, 4, 1),
        uint3(1, 2, 5),
        uint3(5, 2, 0),
        uint3(0, 2, 4),
        uint3(4, 2, 1),
    };
};

} // namespace GSGI