//
// Created by adamyuan on 2/3/25.
//

#ifndef GSGI_MESHGSTRAINER_TRAIT_DEPTHALBEDO_HPP
#define GSGI_MESHGSTRAINER_TRAIT_DEPTHALBEDO_HPP

#include <Falcor.h>
#include "../MeshGSTrainer.hpp"

using namespace Falcor;

namespace GSGI
{

struct MeshGSTrainDepthAlbedoTrait
{
    static constexpr const char* kIncludePath = "GaussianGI/Algorithm/MeshGSTrainer/Trait/DepthAlbedo.slangh";
    static constexpr uint kFloatsPerSplatAttrib = 3;
    struct SplatAttrib
    {
        float3 albedo;
        void bindShaderData(const ShaderVar& var) const { var["albedo"] = albedo; }
    };
    static constexpr uint kFloatsPerSplatChannel = 4;
    struct SplatChannel
    {
        float3 albedo;
        float depth;
    };

    struct SplatTexture
    {
        ref<Texture> pAlbedoDepthTexture, pTTexture;

        static SplatTexture create(const ref<Device>& pDevice, uint2 resolution);
        void clearUavRsMs(RenderContext* pRenderContext) const;
        void bindShaderData(const ShaderVar& var) const;
        void bindRsMsShaderData(const ShaderVar& var) const;
        bool isCapable(uint2 resolution) const;
    };

    struct SplatRTTexture
    {
        ref<Texture> pAlbedoOneMinusTTexture, pDepthTexture;
        ref<Fbo> pFbo;

        static SplatRTTexture create(const ref<Device>& pDevice, uint2 resolution);
        static BlendState::Desc getBlendStateDesc();
        void clearRtv(RenderContext* pRenderContext) const;
        void bindShaderData(const ShaderVar& var) const;
        bool isCapable(uint2 resolution) const;
        const auto& getFbo() const { return pFbo; }
    };
    struct MeshRTTexture
    {
        ref<Texture> pAlbedoDepthTexture, pDepthBuffer;
        ref<Fbo> pFbo;

        static MeshRTTexture create(const ref<Device>& pDevice, uint2 resolution);
        void clearRtv(RenderContext* pRenderContext) const;
        void bindShaderData(const ShaderVar& var) const;
        bool isCapable(uint2 resolution) const;
        const auto& getFbo() const { return pFbo; }
    };
};

static_assert(Concepts::MeshGSTrainTrait<MeshGSTrainDepthAlbedoTrait>);

} // namespace GSGI

#endif
