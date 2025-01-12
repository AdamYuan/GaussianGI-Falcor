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

    // Compute Passes
    mpForwardViewPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/ForwardView.cs.slang", "csMain");
    mpZeroGradPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/ZeroGrad.cs.slang", "csMain");

    // Raster Passes
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
    mpForwardDrawPass->getState()->setVao(pPointVao);
    mpForwardDrawPass->getState()->setRasterizerState(pSplatRasterState);
    mpForwardDrawPass->getState()->setBlendState(BlendState::create(MeshGSTrainSplatRT<TrainType_V>::getBlendStateDesc()));
    mpForwardDrawPass->getState()->setDepthStencilState(pSplatDepthState);

    mpBackwardDrawPass = RasterPass::create(
        pDevice,
        []
        {
            ProgramDesc desc;
            desc.addShaderLibrary("GaussianGI/Algorithm/MeshGSTrainer/BackwardDraw.3d.slang")
                .vsEntry("vsMain")
                .gsEntry("gsMain")
                .psEntry("psMain");
            return desc;
        }()
    );
    mpBackwardDrawPass->getState()->setVao(pPointVao);
    mpBackwardDrawPass->getState()->setRasterizerState(pSplatRasterState);
    mpBackwardDrawPass->getState()->setDepthStencilState(pSplatDepthState);
    mpBackwardDrawPass->getState()->setViewport(
        0, GraphicsState::Viewport{0.0f, 0.0f, float(mDesc.resolution.x), float(mDesc.resolution.y), 0.0f, 0.0f}
    );
}

template<MeshGSTrainType TrainType_V>
void MeshGSTrainer<TrainType_V>::zeroGrad(RenderContext* pRenderContext, const MeshGSTrainResource<TrainType_V>& resource) const
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::zeroGrad");

    auto [prog, var] = getShaderProgVar(mpZeroGradPass);
    var["gSplatCount"] = mDesc.maxSplatCount;
    resource.splatDLossBuf.bindShaderData(var["gDLossDSplats"]);
    mpZeroGradPass->execute(pRenderContext, mDesc.maxSplatCount, 1, 1);
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
        camera.bindShaderData(var["gCamera"]);
        var["gResolution"] = float2(mDesc.resolution);
        var["gSplatCount"] = mDesc.maxSplatCount;
        resource.splatBuf.bindShaderData(var["gSplats"]);
        var["gSplatViewDrawArgs"] = resource.pSplatViewDrawArgBuffer;
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        resource.splatViewDLossBuf.bindShaderData(var["gDLossDSplatViews"]);
        var["gSplatViewSplatIDs"] = resource.pSplatViewSplatIDBuffer;
        var["gSplatViewSortKeys"] = resource.pSplatViewSortKeyBuffer;
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        var["gSplatViewAxes"] = resource.pSplatViewAxisBuffer;
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
        resource.splatRT.clearRtv(pRenderContext);

        auto [prog, var] = getShaderProgVar(mpForwardDrawPass);
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        var["gSplatViewAxes"] = resource.pSplatViewAxisBuffer;
        var["gResolution"] = float2(mDesc.resolution);

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

template<MeshGSTrainType TrainType_V>
void MeshGSTrainer<TrainType_V>::backward(
    RenderContext* pRenderContext,
    const MeshGSTrainCamera& camera,
    const MeshGSTrainResource<TrainType_V>& resource
)
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::backward");
    {
        FALCOR_PROFILE(pRenderContext, "draw");
        resource.splatTmpTex.clearRsMs(pRenderContext);

        auto [prog, var] = getShaderProgVar(mpBackwardDrawPass);
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        var["gSplatViewAxes"] = resource.pSplatViewAxisBuffer;
        var["gResolution"] = float2(mDesc.resolution);
        resource.splatDLossTex.bindShaderData(var["gDLossDCs_Ts"]);
        resource.splatTmpTex.bindShaderData(var["gRs_Ms"]);
        resource.splatViewDLossBuf.bindShaderData(var["gDLossDSplatViews"]);

        pRenderContext->drawIndirect(
            mpBackwardDrawPass->getState().get(),
            mpBackwardDrawPass->getVars().get(),
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