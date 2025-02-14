#ifndef GSGI_ICOSAHEDRON_HPP
#define GSGI_ICOSAHEDRON_HPP

#include <Falcor.h>
#include <array>

using namespace Falcor;

namespace GSGI
{

struct Icosahedron
{
#define ICOSAHEDRON_X .525731112119133606
#define ICOSAHEDRON_Z .850650808352039932
    static constexpr uint kVertexCount = 12;
    static constexpr std::array<float3, kVertexCount> kVertices = {
        float3(-ICOSAHEDRON_X, 0, ICOSAHEDRON_Z),
        float3(ICOSAHEDRON_X, 0, ICOSAHEDRON_Z),
        float3(-ICOSAHEDRON_X, 0, -ICOSAHEDRON_Z),
        float3(ICOSAHEDRON_X, 0, -ICOSAHEDRON_Z),
        float3(0, ICOSAHEDRON_Z, ICOSAHEDRON_X),
        float3(0, ICOSAHEDRON_Z, -ICOSAHEDRON_X),
        float3(0, -ICOSAHEDRON_Z, ICOSAHEDRON_X),
        float3(0, -ICOSAHEDRON_Z, -ICOSAHEDRON_X),
        float3(ICOSAHEDRON_Z, ICOSAHEDRON_X, 0),
        float3(-ICOSAHEDRON_Z, ICOSAHEDRON_X, 0),
        float3(ICOSAHEDRON_Z, -ICOSAHEDRON_X, 0),
        float3(-ICOSAHEDRON_Z, -ICOSAHEDRON_X, 0)
    };
#undef ICOSAHEDRON_X
#undef ICOSAHEDRON_Z

    static constexpr uint kTriangleCount = 20;
    static constexpr std::array<uint3, kTriangleCount> kTriangles = {
        uint3(0, 4, 1), uint3(0, 9, 4),  uint3(9, 5, 4),  uint3(4, 5, 8),  uint3(4, 8, 1),  uint3(8, 10, 1), uint3(8, 3, 10),
        uint3(5, 3, 8), uint3(5, 2, 3),  uint3(2, 7, 3),  uint3(7, 10, 3), uint3(7, 6, 10), uint3(7, 11, 6), uint3(11, 0, 6),
        uint3(0, 1, 6), uint3(6, 1, 10), uint3(9, 0, 11), uint3(9, 11, 2), uint3(9, 2, 5),  uint3(7, 2, 11),
    };

    static constexpr uint kTriStripIndexCount = 34;
    static constexpr uint kTriStripSegmentCount = 7;
    static constexpr uint kTriStripRestart = uint(-1);
    static constexpr uint kTriStripCount = kTriStripIndexCount + kTriStripSegmentCount;
    static constexpr std::array<uint, kTriStripCount> kTriStrips = {
        0,
        4,
        1,
        8,
        10,
        3,
        7,
        2,
        11,
        9,
        0,
        4,
        kTriStripRestart, //
        11,
        0,
        6,
        1,
        kTriStripRestart, //
        10,
        7,
        6,
        11,
        kTriStripRestart, //
        6,
        1,
        10,
        kTriStripRestart, //
        2,
        5,
        9,
        4,
        kTriStripRestart, //
        8,
        5,
        3,
        2,
        kTriStripRestart, //
        4,
        5,
        8,
        kTriStripRestart, //
    };
};

} // namespace GSGI

#endif