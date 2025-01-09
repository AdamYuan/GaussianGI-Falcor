//
// Created by adamyuan on 1/8/25.
//

#include "MeshGSTrainer.hpp"

#include "../../Util/ShaderUtil.hpp"

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
void MeshGSTrainer<TrainType_V>::forward(
    RenderContext* pRenderContext,
    const MeshGSTrainCamera& camera,
    const MeshGSTrainResource<TrainType_V>& resource,
    const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
    const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
) const
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::forward");
    {
        FALCOR_PROFILE(pRenderContext, "view");

        auto [prog, var] = getShaderProgVar(mpForwardViewPass);
        camera.bindShaderData(var["gCamera"], float2(mDesc.resolution));
        var["gSplatCount"] = mDesc.maxSplatCount;
        resource.splatBuf.bindShaderData(var["gSplats"]);
        var["gSplatViewDrawArgs"] = resource.pSplatViewDrawArgBuffer;
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        var["gSplatViewSplatIDs"] = resource.pSplatViewSplatIDBuffer;
        var["gSplatViewSortKeys"] = resource.pSplatViewSortKeyBuffer;
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        // Reset counter (pRenderContext->updateBuffer)
        static_assert(offsetof(DrawArguments, InstanceCount) == sizeof(uint));
        resource.pSplatViewDrawArgBuffer->template setElement<uint>(offsetof(DrawArguments, InstanceCount) / sizeof(uint), 0);
        // Dispatch
        mpForwardViewPass->execute(pRenderContext, mDesc.maxSplatCount, 1, 1);
    }
    {
        FALCOR_PROFILE(pRenderContext, "sortView");
        sorter.dispatch(
            pRenderContext,
            {resource.pSplatViewSortKeyBuffer, resource.pSplatViewSortPayloadBuffer},
            resource.pSplatViewDrawArgBuffer,
            offsetof(DrawArguments, InstanceCount),
            sortResource
        );
    }
    {
        FALCOR_PROFILE(pRenderContext, "draw");
        pRenderContext->clearFbo(resource.splatRT.pFbo.get(), float4{}, 0.0f, 0, FboAttachmentType::Color);

        auto [prog, var] = getShaderProgVar(mpForwardDrawPass);
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        var["gCamResolution"] = float2(mDesc.resolution);

        mpForwardDrawPass->getState()->setFbo(resource.splatRT.pFbo);
        pRenderContext->drawIndirect(
            mpForwardDrawPass->getState().get(),
            mpForwardDrawPass->getVars().get(),
            1,
            resource.pSplatViewDrawArgBuffer.get(),
            0,
            nullptr,
            0
        );
    }
}

template struct MeshGSTrainer<MeshGSTrainType::kDepth>;

} // namespace GSGI