//
// Created by adamyuan on 2/3/25.
//

#ifndef GSGI_MESHGSTRAINER_TRAIT_DEPTHALBEDONORMAL_HPP
#define GSGI_MESHGSTRAINER_TRAIT_DEPTHALBEDONORMAL_HPP

#include <Falcor.h>
#include "../MeshGSTrainer.hpp"

using namespace Falcor;

namespace GSGI
{

struct MeshGSTrainDepthAlbedoNormalTrait
{
    static constexpr const char* kIncludePath = "GaussianGI/Algorithm/MeshGSTrainer/Trait/DepthAlbedoNormal.slangh";
    static constexpr uint kFloatsPerSplatAttrib = 3;
    struct SplatAttrib
    {
        float3 albedo;
        void bindShaderData(const ShaderVar& var) const { var["albedo"] = albedo; }
    };
    static constexpr uint kFloatsPerSplatChannel = 7;
    struct SplatChannel
    {
        float3 albedo;
        float3 normal;
        float depth;
    };

    struct MeshRTTexture
    {
        ref<Texture> pTexture, pDepthBuffer;
        ref<Fbo> pFbo;

        static MeshRTTexture create(const ref<Device>& pDevice, uint2 resolution);
        void clearRtv(RenderContext* pRenderContext) const;
        void bindShaderData(const ShaderVar& var) const;
        bool isCapable(uint2 resolution) const;
        const auto& getFbo() const { return pFbo; }
    };
};

static_assert(Concepts::MeshGSTrainTrait<MeshGSTrainDepthAlbedoNormalTrait>);

} // namespace GSGI

#endif
