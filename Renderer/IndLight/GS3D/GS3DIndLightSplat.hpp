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
    quatf rotate;
    float3 scale;
    float3 albedo;

    static std::vector<GS3DIndLightSplat> loadMesh(const ref<GMesh>& pMesh);
    static void persistMesh(const ref<GMesh>& pMesh, std::span<const GS3DIndLightSplat> splats);
};

struct GS3DIndLightPackedSplatGeom
{
    uint32_t rotate, meanXY;
    uint16_t meanZ;
    float16_t3 scale;

    static GS3DIndLightPackedSplatGeom fromSplat(const GS3DIndLightSplat& splat);
};
static_assert(sizeof(GS3DIndLightPackedSplatGeom) == 4 * sizeof(uint32_t));

struct GS3DIndLightPackedSplatAttrib
{
    uint32_t albedo;

    static GS3DIndLightPackedSplatAttrib fromSplat(const GS3DIndLightSplat& splat);
};
static_assert(sizeof(GS3DIndLightPackedSplatAttrib) == sizeof(uint32_t));

struct GS3DIndLightInstanceDesc
{
    uint firstSplatIdx;
};

struct GS3DIndLightInstancedSplatBuffer
{
    ref<Buffer> pSplatGeomBuffer, pSplatAttribBuffer, pSplatDescBuffer, pInstanceDescBuffer;
    uint32_t splatCount;

    void bindShaderData(const ShaderVar& var) const;
};

} // namespace GSGI

#endif // GSGI_GS3DINDLIGHTSPLAT_HPP
