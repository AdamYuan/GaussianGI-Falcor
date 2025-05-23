//
// Created by adamyuan on 2/1/25.
//

#ifndef GSGI_MESHGSTRAINER_TRAIT_DEPTH_HPP
#define GSGI_MESHGSTRAINER_TRAIT_DEPTH_HPP

#include <Falcor.h>
#include "../MeshGSTrainer.hpp"

using namespace Falcor;

namespace GSGI
{

struct MeshGSTrainDepthTrait
{
    static constexpr const char* kIncludePath = "GaussianGI/Algorithm/MeshGSTrainer/Trait/Depth.slangh";
    static constexpr uint kFloatsPerSplatAttrib = 0;
    struct SplatAttrib
    {
        static void bindShaderData(const ShaderVar&) {}
    };
    static constexpr uint kFloatsPerSplatChannel = 1;
    struct SplatChannel
    {
        float depth;
    };

    struct MeshRTTexture
    {
        ref<Texture> pDepthTexture, pDepthBuffer;
        ref<Fbo> pFbo;

        static MeshRTTexture create(const ref<Device>& pDevice, uint2 resolution);
        void clearRtv(RenderContext* pRenderContext) const;
        void bindShaderData(const ShaderVar& var) const;
        bool isCapable(uint2 resolution) const;
        const auto& getFbo() const { return pFbo; }
    };
};

static_assert(Concepts::MeshGSTrainTrait<MeshGSTrainDepthTrait>);

} // namespace GSGI

#endif
