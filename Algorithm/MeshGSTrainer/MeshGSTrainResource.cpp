//
// Created by adamyuan on 1/8/25.
//

#include "MeshGSTrainer.hpp"

#include <ranges>
#include "../../Util/TextureUtil.hpp"

namespace GSGI
{

namespace
{
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
template<uint32_t... StructSize_Vs>
std::array<ref<Buffer>, sizeof...(StructSize_Vs)> createBuffers(const ref<Device>& pDevice, uint elementCount, ResourceBindFlags bindFlags)
{
    std::array<ref<Buffer>, sizeof...(StructSize_Vs)> pBuffers;
    uint idx = 0;
    ((pBuffers[idx++] = createBuffer<StructSize_Vs>(pDevice, elementCount, bindFlags)), ...);
    return pBuffers;
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
        // Tex0 RGBA32F: Depth + T (Use RGBA32F instead of RG32F temporarily because slang-gfx don't support RGBA Swizzling)
        // TODO: Modify slang-gfx and Falcor to support RGBA Swizzling
        splatRT.pTextures = createTextures<ResourceFormat::RGBA32Float>(
            pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget
        );
        splatRT.pFbo = Fbo::create(pDevice, {splatRT.pTextures[0]});

        // MeshGSTrainType::kDepthColor:
        // Tex0 RGBA32F: Depth + T (Use RGBA32F instead of RG32F temporarily because slang-gfx don't support RGBA Swizzling)
        // Tex1 RGBA32F: Color + T
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return splatRT;
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainSplatRT<TrainType_V>::bindShaderData(const ShaderVar& var) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
        var["depths_Ts"] = pTextures[0];
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
    MeshGSTrainMeshRT meshRT;
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        // MeshGSTrainType::kDepth
        // R32F: Depth
        meshRT.pTexture = createTexture<ResourceFormat::RGBA32Float>(
            pDevice, resolution, ResourceBindFlags::ShaderResource | ResourceBindFlags::RenderTarget
        );
        meshRT.pFbo = Fbo::create(pDevice, {meshRT.pTexture});

        // MeshGSTrainType::kDepthColor:
        // RGBA32F: Depth + Color
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return meshRT;
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
MeshGSTrainSplatBuf<TrainType_V> MeshGSTrainSplatBuf<TrainType_V>::create(
    const ref<Device>& pDevice,
    uint splatCount,
    const InitData& initData
)
{
    MeshGSTrainSplatBuf splatBuf;
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        // MeshGSTrainType::kDepth
        // Quat(4) + Mean(3) + Scale(3) = 4 + 4 + 2
        splatBuf.pBuffers[0] = createBuffer<sizeof(float4)>(
            pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, initData.getData(0, splatCount)
        );
        splatBuf.pBuffers[1] = createBuffer<sizeof(float4)>(
            pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, initData.getData(1, splatCount)
        );
        splatBuf.pBuffers[2] = createBuffer<sizeof(float2)>(
            pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, initData.getData(2, splatCount)
        );
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return splatBuf;
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainSplatBuf<TrainType_V>::bindShaderData(const ShaderVar& var) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        var["rotates"] = pBuffers[0];
        var["means_scaleXs"] = pBuffers[1];
        var["scaleYZs"] = pBuffers[2];
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
}
template<MeshGSTrainType TrainType_V>
bool MeshGSTrainSplatBuf<TrainType_V>::isCapable(uint splatCount) const
{
    return isBufferCapable(splatCount, pBuffers);
}

template<MeshGSTrainType TrainType_V>
MeshGSTrainSplatAdamBuf<TrainType_V> MeshGSTrainSplatAdamBuf<TrainType_V>::create(const ref<Device>& pDevice, uint splatCount)
{
    MeshGSTrainSplatAdamBuf splatAdamBuf;
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        // MeshGSTrainType::kDepth
        // 2 * (Quat(4) + Mean(3) + Scale(3)) = 4 * 5
        splatAdamBuf.pBuffers = createBuffers<sizeof(float4), sizeof(float4), sizeof(float4), sizeof(float4), sizeof(float4)>(
            pDevice, splatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
        );
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return splatAdamBuf;
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainSplatAdamBuf<TrainType_V>::bindShaderData(const ShaderVar& var) const
{
    // TODO:
}
template<MeshGSTrainType TrainType_V>
bool MeshGSTrainSplatAdamBuf<TrainType_V>::isCapable(uint splatCount) const
{
    return isBufferCapable(splatCount, pBuffers);
}

template<MeshGSTrainType TrainType_V>
MeshGSTrainSplatViewBuf<TrainType_V> MeshGSTrainSplatViewBuf<TrainType_V>::create(const ref<Device>& pDevice, uint splatViewCount)
{
    MeshGSTrainSplatViewBuf splatViewBuf;
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        // MeshGSTrainType::kDepth
        // clipMean(2) + depth(1) + axis(2) + scale(2) = 4 + 3
        splatViewBuf.pBuffers = createBuffers<sizeof(float4), sizeof(float4)>(
            pDevice, splatViewCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
        );
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
    return splatViewBuf;
}
template<MeshGSTrainType TrainType_V>
void MeshGSTrainSplatViewBuf<TrainType_V>::bindShaderData(const ShaderVar& var) const
{
    if constexpr (TrainType_V == MeshGSTrainType::kDepth)
    {
        var["clipMeans_depths"] = pBuffers[0];
        var["axes_scales"] = pBuffers[1];
    }
    else
        FALCOR_CHECK(false, "Unimplemented");
}
template<MeshGSTrainType TrainType_V>
bool MeshGSTrainSplatViewBuf<TrainType_V>::isCapable(uint splatViewCount) const
{
    return isBufferCapable(splatViewCount, pBuffers);
}

template<MeshGSTrainType TrainType_V>
MeshGSTrainResource<TrainType_V> MeshGSTrainResource<TrainType_V>::create(
    const ref<Device>& pDevice,
    uint splatCount,
    uint2 resolution,
    const typename MeshGSTrainSplatBuf<TrainType_V>::InitData& splatInitData
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
    return MeshGSTrainResource{
        .splatRT = MeshGSTrainSplatRT<TrainType_V>::create(pDevice, resolution),
        .meshRT = MeshGSTrainMeshRT<TrainType_V>::create(pDevice, resolution),
        .splatDLossTex = MeshGSTrainSplatTex<TrainType_V>::create(pDevice, resolution),
        .splatTmpTex = MeshGSTrainSplatTex<TrainType_V>::create(pDevice, resolution),
        .splatBuf = MeshGSTrainSplatBuf<TrainType_V>::create(pDevice, splatCount, splatInitData),
        .splatDLossBuf = MeshGSTrainSplatBuf<TrainType_V>::create(pDevice, splatCount),
        .splatAdamBuf = MeshGSTrainSplatAdamBuf<TrainType_V>::create(pDevice, splatCount),
        .splatViewBuf = MeshGSTrainSplatViewBuf<TrainType_V>::create(pDevice, splatCount),
        .splatViewDLossBuf = MeshGSTrainSplatViewBuf<TrainType_V>::create(pDevice, splatCount),
        /* .sortResource = DeviceSortResource<DeviceSortDispatchType::kIndirect>::create(
            pDevice, DeviceSortDesc({DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload}), splatCount
        ), */
        .pSplatViewCountBuffer =
            createBuffer<sizeof(uint)>(pDevice, 1, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess),
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
template<MeshGSTrainType TrainType_V>
bool MeshGSTrainResource<TrainType_V>::isCapable(uint splatCount, uint2 resolution) const
{
    return isResourceCapable(
               splatCount,
               resolution,
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
           isBufferCapable(splatCount, pSplatViewSplatIDBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer) &&
           isBufferCapable(1, pSplatViewCountBuffer, pSplatViewDrawArgBuffer, pSplatViewDispatchArgBuffer);
    // Don't need to check sortResource since it is checked in DeviceSorter
}

template struct MeshGSTrainSplatRT<MeshGSTrainType::kDepth>;
template struct MeshGSTrainMeshRT<MeshGSTrainType::kDepth>;
template struct MeshGSTrainSplatTex<MeshGSTrainType::kDepth>;
template struct MeshGSTrainSplatBuf<MeshGSTrainType::kDepth>;
template struct MeshGSTrainSplatAdamBuf<MeshGSTrainType::kDepth>;
template struct MeshGSTrainSplatViewBuf<MeshGSTrainType::kDepth>;
template struct MeshGSTrainResource<MeshGSTrainType::kDepth>;

} // namespace GSGI
