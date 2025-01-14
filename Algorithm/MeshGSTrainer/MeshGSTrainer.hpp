//
// Created by adamyuan on 1/8/25.
//

#ifndef GSGI_MESHGSTRAINER_HPP
#define GSGI_MESHGSTRAINER_HPP

#include <Falcor.h>
#include <Core/Pass/RasterPass.h>
#include "../DeviceSort/DeviceSorter.hpp"
#include "../../Util/SOABufferUtil.hpp"

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
    uint batchSize;
};

struct MeshGSTrainState
{
    uint iteration, batch;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplat
{
    quatf rotate;
    float3 mean{};
    float3 scale{};
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
    void bindShaderData(const ShaderVar& var) const
    {
        var["viewMat"] = viewMat;
        var["projMat"] = projMat;
        var["nearZ"] = nearZ;
        var["farZ"] = farZ;
    }
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatRT
{
    static constexpr uint kTextureCount = TrainType_V == MeshGSTrainType::kDepth ? 2 : 3;
    std::array<ref<Texture>, kTextureCount> pTextures;
    ref<Fbo> pFbo;

    static MeshGSTrainSplatRT create(const ref<Device>& pDevice, uint2 resolution);
    static BlendState::Desc getBlendStateDesc();
    void clearRtv(RenderContext* pRenderContext) const;
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint2 resolution) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainMeshRT
{
    ref<Texture> pTexture, pDepthBuffer;
    ref<Fbo> pFbo;

    static MeshGSTrainMeshRT create(const ref<Device>& pDevice, uint2 resolution);
    void clearRtv(RenderContext* pRenderContext) const;
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint2 resolution) const;
};

namespace Concepts
{
template<typename T, MeshGSTrainType TrainType_V>
concept MeshGSTrainDataset =
    requires(const T& t) { t.generate(std::declval<RenderContext*>(), MeshGSTrainMeshRT<TrainType_V>{}, MeshGSTrainCamera{}); };
} // namespace Concepts

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatTex
{
    ref<Texture> pTexture;

    static MeshGSTrainSplatTex create(const ref<Device>& pDevice, uint2 resolution);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint2 resolution) const;
    void clearUAVRsMs(RenderContext* pRenderContext) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatAdamBuf
{
    static constexpr uint kBufferCount = TrainType_V == MeshGSTrainType::kDepth ? 5 : 7;
    std::array<ref<Buffer>, kBufferCount> pBuffers;

    static MeshGSTrainSplatAdamBuf create(const ref<Device>& pDevice, uint splatCount);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint splatCount) const;
    void clearUAV(RenderContext* pRenderContext) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainSplatViewBuf
{
    static constexpr uint kBufferCount = TrainType_V == MeshGSTrainType::kDepth ? 2 : 3;
    std::array<ref<Buffer>, kBufferCount> pBuffers;

    static MeshGSTrainSplatViewBuf create(const ref<Device>& pDevice, uint splatViewCount);
    void bindShaderData(const ShaderVar& var) const;
    bool isCapable(uint splatViewCount) const;
    void clearUAV(RenderContext* pRenderContext) const;
};

template<MeshGSTrainType TrainType_V>
struct MeshGSTrainResource
{
    using SplatSOAUnitTrait = SOAUnitTrait<float, 4>;
    using SplatSOATrait = SOATrait<SplatSOAUnitTrait, 10>;
    using SplatViewSOATrait = SOATrait<SplatSOAUnitTrait, 6>;
    using SplatAdamSOATrait = SOATrait<SplatSOAUnitTrait, 20>;

    MeshGSTrainSplatRT<TrainType_V> splatRT;
    MeshGSTrainMeshRT<TrainType_V> meshRT;
    MeshGSTrainSplatTex<TrainType_V> splatDLossTex, splatTmpTex;
    SOABuffer<SplatSOATrait> splatBuf, splatDLossBuf;
    MeshGSTrainSplatAdamBuf<TrainType_V> splatAdamBuf;
    MeshGSTrainSplatViewBuf<TrainType_V> splatViewBuf, splatViewDLossBuf;
    ref<Buffer> pSplatViewSplatIDBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer, pSplatViewAxisBuffer;
    ref<Buffer> pSplatViewDrawArgBuffer, pSplatViewDispatchArgBuffer;

    static MeshGSTrainResource create(
        const ref<Device>& pDevice,
        uint splatCount,
        uint2 resolution,
        const SOABufferInitData<SplatSOATrait>& splatInitData = {}
    );
    static MeshGSTrainResource create(
        const ref<Device>& pDevice,
        const MeshGSTrainDesc& desc,
        const SOABufferInitData<SplatSOATrait>& splatInitData = {}
    )
    {
        return create(pDevice, desc.maxSplatCount, desc.resolution, splatInitData);
    }
    static SOABuffer<SplatSOATrait> createSplatBuffer(
        const ref<Device>& pDevice,
        uint splatCount,
        const SOABufferInitData<SplatSOATrait>& splatInitData = {}
    );
    bool isCapable(uint splatCount, uint2 resolution) const;
    bool isCapable(const MeshGSTrainDesc& desc) const { return isCapable(desc.maxSplatCount, desc.resolution); }
};

template<MeshGSTrainType TrainType_V>
class MeshGSTrainer
{
private:
    MeshGSTrainDesc mDesc{};
    ref<ComputePass> mpForwardViewPass, mpBackwardViewPass, mpBackwardCmdPass, mpOptimizePass, mpLossPass;
    ref<RasterPass> mpForwardDrawPass, mpBackwardDrawPass;

    void iterate(
        MeshGSTrainState& state,
        RenderContext* pRenderContext,
        const MeshGSTrainResource<TrainType_V>& resource,
        const MeshGSTrainCamera& camera,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const;

    void reset(RenderContext* pRenderContext, const MeshGSTrainResource<TrainType_V>& resource) const;
    void forward(
        RenderContext* pRenderContext,
        const MeshGSTrainResource<TrainType_V>& resource,
        const MeshGSTrainCamera& camera,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const;
    void loss(RenderContext* pRenderContext, const MeshGSTrainResource<TrainType_V>& resource) const;
    void backward(RenderContext* pRenderContext, const MeshGSTrainResource<TrainType_V>& resource, const MeshGSTrainCamera& camera) const;
    void optimize(RenderContext* pRenderContext, const MeshGSTrainResource<TrainType_V>& resource) const;
    static void generateData(
        RenderContext* pRenderContext,
        const MeshGSTrainResource<TrainType_V>& resource,
        const MeshGSTrainCamera& camera,
        const Concepts::MeshGSTrainDataset<TrainType_V> auto& dataset
    )
    {
        FALCOR_PROFILE(pRenderContext, "MeshGSTrainer::genData");
        resource.meshRT.clearRtv(pRenderContext);
        dataset.generate(pRenderContext, resource.meshRT, camera);
    }

public:
    MeshGSTrainer() = default;
    MeshGSTrainer(const ref<Device>& pDevice, const MeshGSTrainDesc& desc);
    static DeviceSortDesc getSortDesc() { return DeviceSortDesc({DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload}); }

    const auto& getDesc() const { return mDesc; }

    void iterate(
        MeshGSTrainState& state,
        RenderContext* pRenderContext,
        const MeshGSTrainResource<TrainType_V>& resource,
        const MeshGSTrainCamera& camera,
        const Concepts::MeshGSTrainDataset<TrainType_V> auto& dataset,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const
    {
        generateData(pRenderContext, resource, camera, dataset);
        iterate(state, pRenderContext, resource, camera, sorter, sortResource);
    }
};

} // namespace GSGI

#endif // GSGI_MESHGSTRAINER_HPP
