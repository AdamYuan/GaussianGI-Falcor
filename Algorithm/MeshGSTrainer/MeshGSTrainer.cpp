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
    mDesc.batchSize = math::max(mDesc.batchSize, 1u);

    auto pPointVao = Vao::create(Vao::Topology::PointList);

    // Compute Passes
    mpForwardViewPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/ForwardView.cs.slang", "csMain");
    mpLossPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/Loss.cs.slang", "csMain");
    mpBackwardCmdPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/BackwardCmd.cs.slang", "csMain");
    DefineList defList;
    defList.add("BATCH_SIZE", fmt::to_string(mDesc.batchSize));
    mpBackwardViewPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/BackwardView.cs.slang", "csMain", defList);
    mpOptimizePass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/Optimize.cs.slang", "csMain", defList);

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
void MeshGSTrainer<TrainType_V>::iterate(
    MeshGSTrainState& state,
    RenderContext* pRenderContext,
    const MeshGSTrainResource<TrainType_V>& resource,
    const MeshGSTrainCamera& camera,
    const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
    const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
) const
{
    if (state.iteration == 0)
        reset(pRenderContext, resource);

    forward(pRenderContext, resource, camera, sorter, sortResource);
    loss(pRenderContext, resource);
    backward(pRenderContext, resource, camera);

    if ((++state.iteration) % mDesc.batchSize == 0)
    {
        state.adamBeta1T *= mDesc.adamBeta1;
        state.adamBeta2T *= mDesc.adamBeta2;
        optimize(state, pRenderContext, resource);
        ++state.batch;
    }
}

template<MeshGSTrainType TrainType_V>
void MeshGSTrainer<TrainType_V>::reset(RenderContext* pRenderContext, const MeshGSTrainResource<TrainType_V>& resource) const
{
    resource.splatDLossBuf.clearUAV(pRenderContext);
    resource.splatViewDLossBuf.clearUAV(pRenderContext);
    resource.splatAdamBuf.clearUAV(pRenderContext);
}

template<MeshGSTrainType TrainType_V>
void MeshGSTrainer<TrainType_V>::forward(
    RenderContext* pRenderContext,
    const MeshGSTrainResource<TrainType_V>& resource,
    const MeshGSTrainCamera& camera,
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
        // resource.splatViewDLossBuf.bindShaderData(var["gDLossDSplatViews"]);
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
void MeshGSTrainer<TrainType_V>::loss(RenderContext* pRenderContext, const MeshGSTrainResource<TrainType_V>& resource) const
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::loss");

    auto [prog, var] = getShaderProgVar(mpLossPass);
    var["gResolution"] = uint2(mDesc.resolution);
    resource.splatRT.bindShaderData(var["gSplatRT"]);
    resource.meshRT.bindShaderData(var["gMeshRT"]);
    resource.splatDLossTex.bindShaderData(var["gDLossDCs_Ts"]);
    mpLossPass->execute(pRenderContext, mDesc.resolution.x, mDesc.resolution.y, 1);
}

template<MeshGSTrainType TrainType_V>
void MeshGSTrainer<TrainType_V>::backward(
    RenderContext* pRenderContext,
    const MeshGSTrainResource<TrainType_V>& resource,
    const MeshGSTrainCamera& camera
) const
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::backward");
    {
        FALCOR_PROFILE(pRenderContext, "draw");
        resource.splatTmpTex.clearUAVRsMs(pRenderContext);

        auto [prog, var] = getShaderProgVar(mpBackwardDrawPass);
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        var["gSplatViewAxes"] = resource.pSplatViewAxisBuffer;
        var["gResolution"] = float2(mDesc.resolution);
        resource.splatDLossTex.bindShaderData(var["gDLossDCs_Ts"]);
        var["gRs_Ms"] = resource.splatTmpTex.pTexture; // bindShaderData(var["gRs_Ms"]);
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
    {
        FALCOR_PROFILE(pRenderContext, "cmd");

        auto [prog, var] = getShaderProgVar(mpBackwardCmdPass);
        var["gSplatViewDrawArgs"] = resource.pSplatViewDrawArgBuffer;
        var["gSplatViewDispatchArgs"] = resource.pSplatViewDispatchArgBuffer;
        mpBackwardCmdPass->execute(pRenderContext, 1, 1, 1);
    }
    {
        FALCOR_PROFILE(pRenderContext, "view");

        auto [prog, var] = getShaderProgVar(mpBackwardViewPass);
        camera.bindShaderData(var["gCamera"]);
        var["gResolution"] = float2(mDesc.resolution);

        var["gSplatViewDrawArgs"] = resource.pSplatViewDrawArgBuffer;
        resource.splatViewDLossBuf.bindShaderData(var["gDLossDSplatViews"]);
        resource.splatBuf.bindShaderData(var["gSplats"]);
        resource.splatDLossBuf.bindShaderData(var["gDLossDSplats"]);
        var["gSplatViewSplatIDs"] = resource.pSplatViewSplatIDBuffer;

        mpBackwardViewPass->executeIndirect(pRenderContext, resource.pSplatViewDispatchArgBuffer.get());
    }
}

template<MeshGSTrainType TrainType_V>
void MeshGSTrainer<TrainType_V>::optimize(
    const MeshGSTrainState& state,
    RenderContext* pRenderContext,
    const MeshGSTrainResource<TrainType_V>& resource
) const
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::optimize");

    auto [prog, var] = getShaderProgVar(mpOptimizePass);
    var["gSplatCount"] = mDesc.maxSplatCount;
    resource.splatBuf.bindShaderData(var["gSplats"]);
    resource.splatDLossBuf.bindShaderData(var["gDLossDSplats"]);
    resource.splatAdamBuf.bindShaderData(var["gSplatAdams"]);
    var["gAdamBeta1"] = mDesc.adamBeta1;
    var["gAdamBeta2"] = mDesc.adamBeta2;
    var["gAdamBeta1T"] = state.adamBeta1T;
    var["gAdamBeta2T"] = state.adamBeta2T;
    var["gAdamLearnRate"] = mDesc.adamLearnRate;
    var["gAdamEpsilon"] = mDesc.adamEpsilon;
    mpOptimizePass->execute(pRenderContext, mDesc.maxSplatCount, 1, 1);
}

template struct MeshGSTrainer<MeshGSTrainType::kDepth>;

} // namespace GSGI