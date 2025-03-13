//
// Created by adamyuan on 1/8/25.
//
#pragma once

#include "MeshGSTrainer.hpp"

#include <ranges>
#include "../../Util/TextureUtil.hpp"
#include "../../Util/ShaderUtil.hpp"
#include "MeshGSTrainer.slangh"

namespace GSGI
{

namespace
{

template<ResourceFormat Format_V>
ref<Texture> createTexture(const ref<Device>& pDevice, uint2 resolution, ResourceBindFlags bindFlags)
{
    return pDevice->createTexture2D(resolution.x, resolution.y, Format_V, 1, 1, nullptr, bindFlags);
}
bool isTextureCapable(uint2 resolution)
{
    return true;
}
template<typename Texture_T, typename... OtherTexture_Ts>
bool isTextureCapable(uint2 resolution, const Texture_T& pTexture, const OtherTexture_Ts&... pOtherTextures)
{
    return pTexture && math::all(getTextureResolution2(pTexture) == resolution) && isTextureCapable(resolution, pOtherTextures...);
}
template<typename Texture_T, std::size_t TextureCount_V, typename... OtherTexture_Ts>
bool isTextureCapable(uint2 resolution, const std::array<Texture_T, TextureCount_V>& pTextures, const OtherTexture_Ts&... pOtherTextures)
{
    return std::ranges::all_of(
               pTextures, [=](const auto& pTexture) { return pTexture && math::all(getTextureResolution2(pTexture) == resolution); }
           ) &&
           isTextureCapable(resolution, pOtherTextures...);
}

template<uint32_t StructSize_V>
ref<Buffer> createBuffer(const ref<Device>& pDevice, uint elementCount, ResourceBindFlags bindFlags, void* pInitData = nullptr)
{
    return pDevice->createStructuredBuffer(StructSize_V, elementCount, bindFlags, MemoryType::DeviceLocal, pInitData, false);
}
bool isBufferCapable(uint elementCount)
{
    return true;
}
template<typename... OtherBuffer_Ts>
bool isBufferCapable(uint elementCount, const ref<Buffer>& pBuffer, const OtherBuffer_Ts&... pOtherBuffers)
{
    return pBuffer && pBuffer->getElementCount() >= elementCount && isBufferCapable(elementCount, pOtherBuffers...);
}
template<std::size_t BufferCount_V, typename... OtherBuffer_Ts>
bool isBufferCapable(uint elementCount, const std::array<ref<Buffer>, BufferCount_V>& pBuffers, const OtherBuffer_Ts&... pOtherBuffers)
{
    return std::ranges::all_of(
               pBuffers, [=](const ref<Buffer>& pBuffer) { return pBuffer && pBuffer->getElementCount() >= elementCount; }
           ) &&
           isBufferCapable(elementCount, pOtherBuffers...);
}

bool isResourceCapable(uint elementCount, uint2 resolution)
{
    return true;
}
bool isResourceCapable(uint elementCount, uint2 resolution, const auto& firstResource, const auto&... otherResources)
{
    bool isFirstCapable;
    if constexpr (requires { firstResource.isCapable(resolution); })
        isFirstCapable = firstResource.isCapable(resolution);
    else if constexpr (requires { firstResource.isCapable(elementCount); })
        isFirstCapable = firstResource.isCapable(elementCount);
    else
        isFirstCapable = firstResource != nullptr;

    return isFirstCapable && isResourceCapable(elementCount, resolution, otherResources...);
}

} // namespace

template<Concepts::MeshGSTrainTrait Trait_T>
typename MeshGSTrainer<Trait_T>::Resource MeshGSTrainer<Trait_T>::Resource::create(
    const ref<Device>& pDevice,
    uint splatCount,
    uint2 resolution,
    const SplatBufferData& splatInitData
)
{
    DrawArguments drawArgs = {
        .VertexCountPerInstance = 1,
        .InstanceCount = 0,
        .StartVertexLocation = 0,
        .StartInstanceLocation = 0,
    };
    DispatchArguments dispatchArgs = {
        .ThreadGroupCountX = 0,
        .ThreadGroupCountY = 1,
        .ThreadGroupCountZ = 1,
    };
    static_assert(sizeof(float16_t4) == sizeof(uint32_t) * 2); // For SplatViewAxis
    return Resource{
        // .meshRT = Trait_T::MeshRTTexture::create(pDevice, resolution),
        .splatTex = SplatTexture{pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess},
        .splatDLossTex = SplatTexture{pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess},
        .splatTmpTex = SplatTexture{pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess},
        .splatBuf = createSplatBuffer(pDevice, splatCount, splatInitData),
        .splatDLossBuf = createSplatBuffer(pDevice, splatCount),
        .splatAdamBuf = createSplatAdamBuffer(pDevice, splatCount),
        .splatViewBuf = createSplatViewBuffer(pDevice, splatCount),
        .splatViewDLossBuf = createSplatViewBuffer(pDevice, splatCount),
        .splatQuadBuf = SplatQuadBuffer{pDevice, splatCount, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource},
        .pSplatViewSplatIDBuffer =
            createBuffer<sizeof(uint)>(pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess),
        .pSplatViewSortKeyBuffer =
            createBuffer<sizeof(uint)>(pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess),
        .pSplatViewSortPayloadBuffer =
            createBuffer<sizeof(uint)>(pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess),
        .pSplatViewDrawArgBuffer = createBuffer<sizeof(DrawArguments)>(
            pDevice, //
            1,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::IndirectArg,
            &drawArgs
        ),
        .pSplatViewDispatchArgBuffer = createBuffer<sizeof(DispatchArguments)>(
            pDevice,
            1,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::IndirectArg,
            &dispatchArgs
        ),
    };
}
template<Concepts::MeshGSTrainTrait Trait_T>
typename MeshGSTrainer<Trait_T>::SplatBuffer MeshGSTrainer<Trait_T>::Resource::createSplatBuffer(
    const ref<Device>& pDevice,
    uint splatCount,
    const SplatBufferData& splatInitData
)
{
    FALCOR_CHECK(splatInitData.isEmpty() || splatInitData.isCapable(splatCount), "");
    return {pDevice, splatCount, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource, splatInitData};
}
template<Concepts::MeshGSTrainTrait Trait_T>
typename MeshGSTrainer<Trait_T>::SplatAdamBuffer MeshGSTrainer<Trait_T>::Resource::createSplatAdamBuffer(
    const ref<Device>& pDevice,
    uint splatCount
)
{
    return {pDevice, splatCount, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource};
}
template<Concepts::MeshGSTrainTrait Trait_T>
typename MeshGSTrainer<Trait_T>::SplatViewBuffer MeshGSTrainer<Trait_T>::Resource::createSplatViewBuffer(
    const ref<Device>& pDevice,
    uint splatViewCount
)
{
    return {pDevice, splatViewCount, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource};
}
template<Concepts::MeshGSTrainTrait Trait_T>
bool MeshGSTrainer<Trait_T>::Resource::isCapable(uint splatCount, uint2 resolution) const
{
    return isResourceCapable(
               splatCount,
               resolution, //
               // meshRT,
               splatTex,
               splatDLossTex,
               splatTmpTex,
               splatBuf,
               splatDLossBuf,
               splatQuadBuf,
               splatAdamBuf,
               splatViewBuf,
               splatViewDLossBuf
           ) &&
           isBufferCapable(splatCount, pSplatViewSplatIDBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer) &&
           isBufferCapable(1, pSplatViewDrawArgBuffer, pSplatViewDispatchArgBuffer);
    // Don't need to check sortResource since it is checked in DeviceSorter
}

template<Concepts::MeshGSTrainTrait Trait_T>
MeshGSTrainer<Trait_T>::MeshGSTrainer(const ref<Device>& pDevice, const Desc& desc) : mDesc{desc}
{
    mDesc.batchSize = math::max(mDesc.batchSize, 1u);

    auto pPointVao = Vao::create(Vao::Topology::PointList);

    // Compute Passes
    DefineList defList;
    defList.add("BATCH_SIZE", fmt::to_string(mDesc.batchSize));
    addDefine(defList);
    mpForwardViewPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/ForwardView.cs.slang", "csMain", defList);
    mpLossPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/Loss.cs.slang", "csMain", defList);
    mpBackwardCmdPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/BackwardCmd.cs.slang", "csMain", defList);
    mpBackwardViewPass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/BackwardView.cs.slang", "csMain", defList);
    mpOptimizePass = ComputePass::create(pDevice, "GaussianGI/Algorithm/MeshGSTrainer/Optimize.cs.slang", "csMain", defList);
    mDesc.learnRate.bindShaderData(mpOptimizePass->getRootVar()["gLearnRate"]);

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
        }(),
        defList
    );
    mpForwardDrawPass->getState()->setVao(pPointVao);
    mpForwardDrawPass->getState()->setRasterizerState(pSplatRasterState);
    mpForwardDrawPass->getState()->setDepthStencilState(pSplatDepthState);
    mpForwardDrawPass->getState()->setViewport(
        0, GraphicsState::Viewport{0.0f, 0.0f, float(mDesc.resolution.x), float(mDesc.resolution.y), 0.0f, 0.0f}
    );

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
        }(),
        defList
    );
    mpBackwardDrawPass->getState()->setVao(pPointVao);
    mpBackwardDrawPass->getState()->setRasterizerState(pSplatRasterState);
    mpBackwardDrawPass->getState()->setDepthStencilState(pSplatDepthState);
    mpBackwardDrawPass->getState()->setViewport(
        0, GraphicsState::Viewport{0.0f, 0.0f, float(mDesc.resolution.x), float(mDesc.resolution.y), 0.0f, 0.0f}
    );
}
template<Concepts::MeshGSTrainTrait Trait_T>
void MeshGSTrainer<Trait_T>::iterate(
    MeshGSTrainState& state,
    RenderContext* pRenderContext,
    const Resource& resource,
    const Data& data,
    const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
    const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
) const
{
    if (state.iteration == 0)
        reset(pRenderContext, resource);

    forward(state, pRenderContext, resource, data.camera, sorter, sortResource);
    loss(pRenderContext, resource, data);
    backward(pRenderContext, resource, data.camera);

    if ((++state.iteration) % mDesc.batchSize == 0)
    {
        state.adamBeta1T *= ADAM_BETA1;
        state.adamBeta2T *= ADAM_BETA2;
        optimize(state, pRenderContext, resource);
        ++state.batch;
    }
}

template<Concepts::MeshGSTrainTrait Trait_T>
void MeshGSTrainer<Trait_T>::reset(RenderContext* pRenderContext, const Resource& resource) const
{
    resource.splatDLossBuf.clearUAV(pRenderContext);
    resource.splatViewDLossBuf.clearUAV(pRenderContext);
    resource.splatAdamBuf.clearUAV(pRenderContext);
}
template<Concepts::MeshGSTrainTrait Trait_T>
void MeshGSTrainer<Trait_T>::forward(
    const MeshGSTrainState& state,
    RenderContext* pRenderContext,
    const Resource& resource,
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
        var["gSplatCount"] = state.splatCount;
        resource.splatBuf.bindShaderData(var["gSplats"]);
        var["gSplatViewDrawArgs"] = resource.pSplatViewDrawArgBuffer;
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        // resource.splatViewDLossBuf.bindShaderData(var["gDLossDSplatViews"]);
        var["gSplatViewSplatIDs"] = resource.pSplatViewSplatIDBuffer;
        var["gSplatViewSortKeys"] = resource.pSplatViewSortKeyBuffer;
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        resource.splatQuadBuf.bindShaderData(var["gSplatQuads"]);
        // Reset counter (pRenderContext->updateBuffer)
        static_assert(offsetof(DrawArguments, InstanceCount) == sizeof(uint));
        resource.pSplatViewDrawArgBuffer->template setElement<uint>(offsetof(DrawArguments, InstanceCount) / sizeof(uint), 0);
        // Dispatch
        mpForwardViewPass->execute(pRenderContext, state.splatCount, 1, 1);
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
        std::array<float, kFloatsPerSplatChannelT> clearSplatRTValue{};
        clearSplatRTValue[kFloatsPerSplatChannelT - 1] = 1.0;
        resource.splatTex.clearTexture(pRenderContext, clearSplatRTValue);

        auto [prog, var] = getShaderProgVar(mpForwardDrawPass);
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        resource.splatTex.bindShaderData(var["gSplatRT"]);
        resource.splatQuadBuf.bindShaderData(var["gSplatQuads"]);
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        var["gCamProjMat"] = camera.projMat;
        var["gCamInvProjMat"] = math::inverse(camera.projMat);

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
template<Concepts::MeshGSTrainTrait Trait_T>
void MeshGSTrainer<Trait_T>::loss(RenderContext* pRenderContext, const Resource& resource, const Data& data) const
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::loss");

    auto [prog, var] = getShaderProgVar(mpLossPass);
    var["gResolution"] = uint2(mDesc.resolution);
    resource.splatTex.bindShaderData(var["gSplatRT"]);
    data.meshRT.bindShaderData(var["gMeshRT"]);
    resource.splatDLossTex.bindShaderData(var["gDLossDCs_Ts"]);
    resource.splatTmpTex.bindShaderData(var["gMs_Ts"]);
    mpLossPass->execute(pRenderContext, mDesc.resolution.x, mDesc.resolution.y, 1);
}
template<Concepts::MeshGSTrainTrait Trait_T>
void MeshGSTrainer<Trait_T>::backward(RenderContext* pRenderContext, const Resource& resource, const MeshGSTrainCamera& camera) const
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::backward");
    {
        FALCOR_PROFILE(pRenderContext, "draw");

        auto [prog, var] = getShaderProgVar(mpBackwardDrawPass);
        resource.splatViewBuf.bindShaderData(var["gSplatViews"]);
        resource.splatQuadBuf.bindShaderData(var["gSplatQuads"]);
        var["gSplatViewSortPayloads"] = resource.pSplatViewSortPayloadBuffer;
        var["gCamProjMat"] = camera.projMat;
        var["gCamInvProjMat"] = math::inverse(camera.projMat);
        resource.splatDLossTex.bindShaderData(var["gDLossDCs_Ts"]);
        resource.splatViewDLossBuf.bindShaderData(var["gDLossDSplatViews"]);
        resource.splatTmpTex.bindShaderData(var["gMs_Ts"]);

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

        var["gSplatViewDrawArgs"] = resource.pSplatViewDrawArgBuffer;
        resource.splatViewDLossBuf.bindShaderData(var["gDLossDSplatViews"]);
        resource.splatBuf.bindShaderData(var["gSplats"]);
        resource.splatDLossBuf.bindShaderData(var["gDLossDSplats"]);
        var["gSplatViewSplatIDs"] = resource.pSplatViewSplatIDBuffer;

        mpBackwardViewPass->executeIndirect(pRenderContext, resource.pSplatViewDispatchArgBuffer.get());
    }
}
template<Concepts::MeshGSTrainTrait Trait_T>
void MeshGSTrainer<Trait_T>::optimize(const MeshGSTrainState& state, RenderContext* pRenderContext, const Resource& resource) const
{
    FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::optimize");

    auto [prog, var] = getShaderProgVar(mpOptimizePass);
    var["gSplatCount"] = state.splatCount;
    resource.splatBuf.bindShaderData(var["gSplats"]);
    resource.splatDLossBuf.bindShaderData(var["gDLossDSplats"]);
    resource.splatAdamBuf.bindShaderData(var["gSplatAdams"]);
    var["gAdamBeta1T"] = state.adamBeta1T;
    var["gAdamBeta2T"] = state.adamBeta2T;
    mpOptimizePass->execute(pRenderContext, state.splatCount, 1, 1);
}

} // namespace GSGI
