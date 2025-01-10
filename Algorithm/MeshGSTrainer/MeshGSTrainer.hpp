//
// Created by adamyuan on 1/8/25.
//

#ifndef GSGI_MESHGSTRAINER_HPP
#define GSGI_MESHGSTRAINER_HPP

#include <Falcor.h>
#include <Core/Pass/RasterPass.h>
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

struct MeshGSTrainSplat
{
    quatf rotate;
    float3 scale;
    float3 mean;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatRT
{
    static constexpr uint kTextureCount = TrainType_V == MeshGSTrainType::kDepth ? 2 : 3;
    std::array<ref<Texture>, kTextureCount> pTextures;
    ref<Fbo> pFbo;

    static MeshGSTrainSplatRT create(const ref<Device>& pDevice, uint2 resolution);
    static BlendState::Desc getBlendStateDesc();
    void clear(RenderContext* pRenderContext) const;
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

    struct InitData
    {
        uint bufferSplatCount;
        std::array<std::vector<float>, kBufferCount> buffers;

        const void* getData(uint idx, uint splatCount) const
        {
            if (buffers[idx].empty())
                return nullptr;
            else
            {
                FALCOR_CHECK(splatCount <= bufferSplatCount, "Read exceed buffer size");
                return buffers[idx].data();
            }
        }
        static InitData create(std::ranges::input_range auto&& splats, uint splatCount)
        {
            InitData initData{.bufferSplatCount = splatCount};
            if constexpr (TrainType_V == MeshGSTrainType::kDepth)
            {
                // MeshGSTrainType::kDepth
                // Quat(4) + Mean(3) + Scale(3) = 4 + 4 + 2
                initData.buffers[0].resize(4 * splatCount);
                initData.buffers[1].resize(4 * splatCount);
                initData.buffers[2].resize(2 * splatCount);
                auto pRotates = reinterpret_cast<float4*>(initData.buffers[0].data());
                auto pMeans_ScaleXs = reinterpret_cast<float4*>(initData.buffers[1].data());
                auto pScaleYZs = reinterpret_cast<float2*>(initData.buffers[2].data());

                for (uint idx = 0; const auto& splat : splats)
                {
                    if (idx >= splatCount)
                        break;
                    pRotates[idx] = float4(splat.rotate.x, splat.rotate.y, splat.rotate.z, splat.rotate.w);
                    pMeans_ScaleXs[idx] = float4(splat.mean.x, splat.mean.y, splat.mean.z, splat.scale.x);
                    pScaleYZs[idx] = float2(splat.scale.y, splat.scale.z);
                    ++idx;
                }
            }
            else
                FALCOR_CHECK(false, "Unimplemented");
            return initData;
        }
    };

    static MeshGSTrainSplatBuf create(const ref<Device>& pDevice, uint splatCount, const InitData& initData = {});
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

struct MeshGSTrainCamera
{
    float4x4 viewMat, projMat;
    float nearZ, farZ;

    static MeshGSTrainCamera create(const Camera& falcorCamera)
    {
        return {
            .viewMat = falcorCamera.getViewMatrix(),
            .projMat = falcorCamera.getProjMatrix(),
            .nearZ = falcorCamera.getNearPlane(),
            .farZ = falcorCamera.getFarPlane(),
        };
    }
    void bindShaderData(const ShaderVar& var, float2 resolution) const
    {
        var["resolution"] = resolution;
        var["viewMat"] = viewMat;
        var["projMat"] = projMat;
        var["nearZ"] = nearZ;
        var["farZ"] = farZ;
    }
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
    ref<Buffer> pSplatViewSplatIDBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer;
    ref<Buffer> pSplatViewDrawArgBuffer, pSplatViewDispatchArgBuffer;

    static MeshGSTrainResource create(
        const ref<Device>& pDevice,
        uint splatCount,
        uint2 resolution,
        const typename MeshGSTrainSplatBuf<TrainType_V>::InitData& splatInitData = {}
    );
    static MeshGSTrainResource create(
        const ref<Device>& pDevice,
        const MeshGSTrainDesc& desc,
        const typename MeshGSTrainSplatBuf<TrainType_V>::InitData& splatInitData = {}
    )
    {
        return create(pDevice, desc.maxSplatCount, desc.resolution, splatInitData);
    }
    bool isCapable(uint splatCount, uint2 resolution) const;
    bool isCapable(const MeshGSTrainDesc& desc) const { return isCapable(desc.maxSplatCount, desc.resolution); }
};

template<MeshGSTrainType TrainType_V>
class MeshGSTrainer
{
private:
    MeshGSTrainDesc mDesc{};
    ref<ComputePass> mpForwardViewPass, mpBackwardViewPass, mpDLossPass, mpAdamPass;
    ref<RasterPass> mpForwardDrawPass, mpBackwardDrawPass;

public:
    MeshGSTrainer() = default;
    MeshGSTrainer(const ref<Device>& pDevice, const MeshGSTrainDesc& desc);
    static DeviceSortDesc getSortDesc() { return DeviceSortDesc({DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload}); }

    const auto& getDesc() const { return mDesc; }

    void forward(
        RenderContext* pRenderContext,
        const MeshGSTrainCamera& camera,
        const MeshGSTrainResource<TrainType_V>& resource,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const;
};

} // namespace GSGI

#endif // GSGI_MESHGSTRAINER_HPP
