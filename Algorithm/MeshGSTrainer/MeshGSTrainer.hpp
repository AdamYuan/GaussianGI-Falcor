//
// Created by adamyuan on 1/8/25.
//

#ifndef GSGI_MESHGSTRAINER_HPP
#define GSGI_MESHGSTRAINER_HPP

#include <Falcor.h>
#include "../DeviceSort/DeviceSorter.hpp"

using namespace Falcor;

namespace GSGI
{

enum class MeshGSTrainType
{
    kDepth,
    kDepthColor,
};

struct MeshGSTrainDesc
{
    uint maxSplatCount;
    uint2 resolution;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatRT
{
    static constexpr uint kTextureCount = TrainType_V == MeshGSTrainType::kDepth ? 1 : 2;
    std::array<ref<Texture>, kTextureCount> pTextures;
    ref<Fbo> pFbo;

    static MeshGSTrainSplatRT create(const ref<Device>& pDevice, uint2 resolution);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint2 resolution) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainMeshRT
{
    ref<Texture> pTexture;
    ref<Fbo> pFbo;

    static MeshGSTrainMeshRT create(const ref<Device>& pDevice, uint2 resolution);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint2 resolution) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatTex
{
    ref<Texture> pTexture;

    static MeshGSTrainSplatTex create(const ref<Device>& pDevice, uint2 resolution);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint2 resolution) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatBuf
{
    static constexpr uint kBufferCount = TrainType_V == MeshGSTrainType::kDepth ? 3 : 4;
    std::array<ref<Buffer>, kBufferCount> pBuffers;

    static MeshGSTrainSplatBuf create(const ref<Device>& pDevice, uint splatCount);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint splatCount) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatAdamBuf
{
    static constexpr uint kBufferCount = TrainType_V == MeshGSTrainType::kDepth ? 5 : 7;
    std::array<ref<Buffer>, kBufferCount> pBuffers;

    static MeshGSTrainSplatAdamBuf create(const ref<Device>& pDevice, uint splatCount);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint splatCount) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatViewBuf
{
    static constexpr uint kBufferCount = TrainType_V == MeshGSTrainType::kDepth ? 2 : 3;
    std::array<ref<Buffer>, kBufferCount> pBuffers;

    static MeshGSTrainSplatViewBuf create(const ref<Device>& pDevice, uint splatViewCount);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint splatViewCount) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainResource
{
    MeshGSTrainSplatRT<TrainType_V> splatRT;
    MeshGSTrainMeshRT<TrainType_V> meshRT;
    MeshGSTrainSplatTex<TrainType_V> splatDLossTex, splatTmpTex;
    MeshGSTrainSplatBuf<TrainType_V> splatBuf, splatDLossBuf;
    MeshGSTrainSplatAdamBuf<TrainType_V> splatAdamBuf;
    MeshGSTrainSplatViewBuf<TrainType_V> splatViewBuf, splatViewDLossBuf;
    // DeviceSortResource<DeviceSortDispatchType::kIndirect> sortResource;
    ref<Buffer> pSplatViewCountBuffer, pSplatViewSplatIDBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer;
    ref<Buffer> pSplatViewDrawArgBuffer, pSplatViewDispatchArgBuffer;

    static MeshGSTrainResource create(const ref<Device>& pDevice, uint splatCount, uint2 resolution);
    static MeshGSTrainResource create(const ref<Device>& pDevice, const MeshGSTrainDesc& desc)
    {
        return create(pDevice, desc.maxSplatCount, desc.resolution);
    }
    bool isCapable(uint splatCount, uint2 resolution) const;
    bool isCapable(const MeshGSTrainDesc& desc) const { return isCapable(desc.maxSplatCount, desc.resolution); }
};

template<MeshGSTrainType TrainType_V>
class MeshGSTrainer
{};

} // namespace GSGI

#endif // GSGI_MESHGSTRAINER_HPP
