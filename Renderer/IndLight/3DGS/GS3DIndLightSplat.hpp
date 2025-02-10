//
// Created by adamyuan on 2/7/25.
//

#ifndef GSGI_GS3DINDLIGHTSPLAT_HPP
#define GSGI_GS3DINDLIGHTSPLAT_HPP

#include "../../../Algorithm/MeshBVH.hpp"

#include <Falcor.h>
#include "../../../Scene/GMesh.hpp"

using namespace Falcor;

namespace GSGI
{

struct GS3DIndLightSplat
{
    float3 mean;
    float16_t4 rotate;
    float16_t3 scale;
    float16_t3 irradiance;

    static std::vector<GS3DIndLightSplat> loadMesh(const ref<GMesh>& pMesh, uint splatCount);
    static void persistMesh(const ref<GMesh>& pMesh, std::span<const GS3DIndLightSplat> splats);
};

static_assert(sizeof(GS3DIndLightSplat) == 8 * sizeof(uint32_t));

struct GS3DIndLightSplatView
{
    float16_t2 axis0, axis1;
    float16_t2 clipXY;
    uint padding;
};

static_assert(sizeof(GS3DIndLightSplatView) == 4 * sizeof(uint32_t));

} // namespace GSGI

#endif // GSGI_GS3DINDLIGHTSPLAT_HPP
