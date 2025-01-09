//
// Created by adamyuan on 1/8/25.
//

#include "MeshGSTrainer.hpp"

namespace GSGI
{

template<MeshGSTrainType TrainType_V>
MeshGSTrainer<TrainType_V>::MeshGSTrainer(const ref<Device>& pDevice, const MeshGSTrainDesc& desc) : mDesc{desc}
{
    auto pPointVao = Vao::create(Vao::Topology::PointList);

    mpForwardViewPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/ForwardView.cs.slang", "csMain");
    mpForwardDrawPass = RasterPass::create(
        pDevice,
        []
        {
            ProgramDesc desc;
            desc.addShaderLibrary("GaussianGI/Algorithm/MeshGSTrainer/ForwardDraw.3d.slang")
                .vsEntry("vsMain")
                .gsEntry("gsMain")
                .psEntry("psMain");
            return desc;
        }()
    );

    ref<DepthStencilState> pSplatDepthState = []
    {
        DepthStencilState::Desc desc;
        desc.setDepthEnabled(false);
        return DepthStencilState::create(desc);
    }();
    ref<RasterizerState> pSplatRasterState = []
    {
        RasterizerState::Desc desc;
        desc.setCullMode(RasterizerState::CullMode::None);
        return RasterizerState::create(desc);
    }();

    mpForwardDrawPass->getState()->setVao(pPointVao);
    mpForwardDrawPass->getState()->setRasterizerState(pSplatRasterState);
    mpForwardDrawPass->getState()->setBlendState(
        []
        {
            BlendState::Desc desc;
            if constexpr (TrainType_V == MeshGSTrainType::kDepth)
            {
                desc.setRtBlend(0, true).setRtParams(
                    0,
                    BlendState::BlendOp::Add,
                    BlendState::BlendOp::Add,
                    BlendState::BlendFunc::SrcAlpha,
                    BlendState::BlendFunc::OneMinusSrcAlpha,
                    BlendState::BlendFunc::One,
                    BlendState::BlendFunc::OneMinusSrcAlpha
                );
            }
            else
                FALCOR_CHECK(false, "Unimplemented");
            return BlendState::create(desc);
        }()
    );
    mpForwardDrawPass->getState()->setDepthStencilState(pSplatDepthState);
}

template<MeshGSTrainType TrainType_V>
void MeshGSTrainer<TrainType_V>::forward(RenderContext* pRenderContext, const MeshGSTrainResource<TrainType_V>& resource) const
{

}

} // namespace GSGI