//
// Created by adamyuan on 1/8/25.
//

#include "MeshGSTrainer.hpp"

#include <ranges>
#include "../../Util/TextureUtil.hpp"
#include "MeshGSTrainer.slangh"

namespace GSGI
{

namespace
{

static_assert(
    sizeof(MeshGSTrainSplat<MeshGSTrainType::kDepth>) ==
    MeshGSTrainResource<MeshGSTrainType::kDepth>::SplatSOATrait::kWordsPerElem * sizeof(float)
);
static_assert(MeshGSTrainResource<MeshGSTrainType::kDepth>::SplatSOATrait::kWordsPerElem == DEPTH_TRAIN_TYPE_SPLAT_FLT_COUNT);
static_assert(MeshGSTrainResource<MeshGSTrainType::kDepth>::SplatViewSOATrait::kWordsPerElem == DEPTH_TRAIN_TYPE_SPLAT_VIEW_FLT_COUNT);
static_assert(MeshGSTrainResource<MeshGSTrainType::kDepth>::SplatAdamSOATrait::kWordsPerElem == DEPTH_TRAIN_TYPE_SPLAT_ADAM_FLT_COUNT);

template<ResourceFormat Format_V>
ref<Texture> createTexture(const ref<Device>& pDevice, uint2 resolution, ResourceBindFlags bindFlags)
{
    return pDevice->createTexture2D(resolution.x, resolution.y, Format_V, 1, 1, nullptr, bindFlags);
}
template<ResourceFormat... Format_Vs>
std::array<ref<Texture>, sizeof...(Format_Vs)> createTextures(const ref<Device>& pDevice, uint2 resolution, ResourceBindFlags bindFlags)
{
    std::array<ref<Texture>, sizeof...(Format_Vs)> pTextures;
    uint idx = 0;
    ((pTextures[idx++] = createTexture<Format_Vs>(pDevice, resolution, bindFlags)), ...);
    return pTextures;
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

template<MeshGSTrainType TrainType_V>
MeshGSTrainSplatRT<TrainType_V> MeshGSTrainSplatRT<TrainType_V>::create(const ref<Device>& pDevice, uint2 resolution)
{
    MeshGSTrainSplatRT splatRT;
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        // MeshGSTrainType::kDepth
        // Tex0 R32F: Depth
        // Tex1 R32F: T
        splatRT.pTextures = createTextures<ResourceFormat::R32Float, ResourceFormat::R32Float>(
            pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget
        );
        splatRT.pFbo = Fbo::create(pDevice, {splatRT.pTextures[0], splatRT.pTextures[1]});

        // MeshGSTrainType::kDepthColor:
        // Tex0 RGBA32F: Color
        // Tex1 R32F: Depth
        // Tex2 R32F: T
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return splatRT;
}
template<MeshGSTrainType TrainType_V>
BlendState::Desc MeshGSTrainSplatRT<TrainType_V>::getBlendStateDesc()
{
    BlendState::Desc desc;
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        desc.setIndependentBlend(true);
        desc.setRtBlend(0, true)
            .setRtParams(
                0,
                BlendState::BlendOp::Add,
                BlendState::BlendOp::Add,
                BlendState::BlendFunc::SrcAlpha,
                BlendState::BlendFunc::OneMinusSrcAlpha,
                BlendState::BlendFunc::One,
                BlendState::BlendFunc::OneMinusSrcAlpha
            )
            .setRenderTargetWriteMask(0, true, false, false, false);
        desc.setRtBlend(1, true)
            .setRtParams(
                1,
                BlendState::BlendOp::Add,
                BlendState::BlendOp::Add, // Ignore
                BlendState::BlendFunc::Zero,
                BlendState::BlendFunc::OneMinusSrcAlpha,
                BlendState::BlendFunc::Zero, // Ignore
                BlendState::BlendFunc::Zero  // Ignore
            )
            .setRenderTargetWriteMask(1, true, false, false, false);
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return desc;
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainSplatRT<TrainType_V>::clearRtv(RenderContext* pRenderContext) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        pRenderContext->clearRtv(pTextures[0]->getRTV().get(), float4{});
        pRenderContext->clearRtv(pTextures[1]->getRTV().get(), float4{1.0f});
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainSplatRT<TrainType_V>::bindShaderData(const ShaderVar& var) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        var["depths"] = pTextures[0];
        var["Ts"] = pTextures[1];
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
}
template<MeshGSTrainType TrainType_V>
bool MeshGSTrainSplatRT<TrainType_V>::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pTextures, pFbo);
}

template<MeshGSTrainType TrainType_V>
MeshGSTrainMeshRT<TrainType_V> MeshGSTrainMeshRT<TrainType_V>::create(const ref<Device>& pDevice, uint2 resolution)
{
    MeshGSTrainMeshRT meshRT{.pDepthBuffer = createTexture<ResourceFormat::D32Float>(pDevice, resolution, ResourceBindFlags::DepthStencil)};
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        // MeshGSTrainType::kDepth
        // R32F: Depth
        meshRT.pTexture = createTexture<ResourceFormat::R32Float>(
            pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget
        );
        meshRT.pFbo = Fbo::create(pDevice, {meshRT.pTexture}, meshRT.pDepthBuffer);

        // MeshGSTrainType::kDepthColor:
        // RGBA32F: Depth + Color
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return meshRT;
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainMeshRT<TrainType_V>::clearRtv(RenderContext* pRenderContext) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        pRenderContext->clearRtv(pTexture->getRTV().get(), float4{});
        pRenderContext->clearDsv(pDepthBuffer->getDSV().get(), 1.0f, 0);
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainMeshRT<TrainType_V>::bindShaderData(const ShaderVar& var) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
        var["depths"] = pTexture;
    else
        FALCOR_CHECK(false, "Unimplemented");
}
template<MeshGSTrainType TrainType_V>
bool MeshGSTrainMeshRT<TrainType_V>::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pTexture, pFbo);
}

template<MeshGSTrainType TrainType_V>
MeshGSTrainSplatTex<TrainType_V> MeshGSTrainSplatTex<TrainType_V>::create(const ref<Device>& pDevice, uint2 resolution)
{
    MeshGSTrainSplatTex splatTex;
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        // MeshGSTrainType::kDepth
        // RG32F: Depth + T
        splatTex.pTexture = createTexture<ResourceFormat::RG32Float>(
            pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
        );

        // MeshGSTrainType::kDepthColor:
        // RGBA32F: Depth + Color(R22G22B20) + T
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return splatTex;
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainSplatTex<TrainType_V>::bindShaderData(const ShaderVar& var) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
        var["depths_Ts"] = pTexture;
    else
        FALCOR_CHECK(false, "Unimplemented");
}
template<MeshGSTrainType TrainType_V>
bool MeshGSTrainSplatTex<TrainType_V>::isCapable(uint2 resolution) const
{
    return isTextureCapable(resolution, pTexture);
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainSplatTex<TrainType_V>::clearUAVRsMs(RenderContext* pRenderContext) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
        pRenderContext->clearTexture(pTexture.get(), float4{0.0f, 1.0f, 0.0f, 0.0f});
    else
        FALCOR_CHECK(false, "Unimplemented");
}

template<MeshGSTrainType TrainType_V>
MeshGSTrainResource<TrainType_V> MeshGSTrainResource<TrainType_V>::create(
    const ref<Device>& pDevice,
    uint splatCount,
    uint2 resolution,
    const SOABufferInitData<SplatSOATrait>& splatInitData
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
    return MeshGSTrainResource{
        .splatRT = MeshGSTrainSplatRT<TrainType_V>::create(pDevice, resolution),
        .meshRT = MeshGSTrainMeshRT<TrainType_V>::create(pDevice, resolution),
        .splatDLossTex = MeshGSTrainSplatTex<TrainType_V>::create(pDevice, resolution),
        .splatTmpTex = MeshGSTrainSplatTex<TrainType_V>::create(pDevice, resolution),
        .splatBuf = createSplatBuffer(pDevice, splatCount, splatInitData),
        .splatDLossBuf = createSplatBuffer(pDevice, splatCount),
        .splatAdamBuf = createSplatAdamBuffer(pDevice, splatCount),
        .splatViewBuf = createSplatViewBuffer(pDevice, splatCount),
        .splatViewDLossBuf = createSplatViewBuffer(pDevice, splatCount),
        .pSplatViewSplatIDBuffer =
            createBuffer<sizeof(uint)>(pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess),
        .pSplatViewSortKeyBuffer =
            createBuffer<sizeof(uint)>(pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess),
        .pSplatViewSortPayloadBuffer =
            createBuffer<sizeof(uint)>(pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess),
        .pSplatViewAxisBuffer =
            createBuffer<sizeof(float16_t4)>(pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess),
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
template<MeshGSTrainType TrainType_V>
SOABuffer<typename MeshGSTrainResource<TrainType_V>::SplatSOATrait> MeshGSTrainResource<TrainType_V>::createSplatBuffer(
    const ref<Device>& pDevice,
    uint splatCount,
    const SOABufferInitData<SplatSOATrait>& splatInitData
)
{
    FALCOR_CHECK(splatInitData.isEmpty() || splatInitData.isCapable(splatCount), "");
    return {pDevice, splatCount, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource, splatInitData};
}
template<MeshGSTrainType TrainType_V>
SOABuffer<typename MeshGSTrainResource<TrainType_V>::SplatAdamSOATrait> MeshGSTrainResource<TrainType_V>::createSplatAdamBuffer(
    const ref<Device>& pDevice,
    uint splatCount
)
{
    return {pDevice, splatCount, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource};
}
template<MeshGSTrainType TrainType_V>
SOABuffer<typename MeshGSTrainResource<TrainType_V>::SplatViewSOATrait> MeshGSTrainResource<TrainType_V>::createSplatViewBuffer(
    const ref<Device>& pDevice,
    uint splatViewCount
)
{
    return {pDevice, splatViewCount, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource};
}
template<MeshGSTrainType TrainType_V>
bool MeshGSTrainResource<TrainType_V>::isCapable(uint splatCount, uint2 resolution) const
{
    return isResourceCapable(
               splatCount,
               resolution, //
               splatRT,
               meshRT,
               splatDLossTex,
               splatTmpTex,
               splatBuf,
               splatDLossBuf,
               splatAdamBuf,
               splatViewBuf,
               splatViewDLossBuf
           ) &&
           isBufferCapable(
               splatCount, pSplatViewSplatIDBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer, pSplatViewAxisBuffer
           ) &&
           isBufferCapable(1, pSplatViewDrawArgBuffer, pSplatViewDispatchArgBuffer);
    // Don't need to check sortResource since it is checked in DeviceSorter
}

template struct MeshGSTrainSplatRT<MeshGSTrainType::kDepth>;
template struct MeshGSTrainMeshRT<MeshGSTrainType::kDepth>;
template struct MeshGSTrainSplatTex<MeshGSTrainType::kDepth>;
template struct MeshGSTrainResource<MeshGSTrainType::kDepth>;

} // namespace GSGI
