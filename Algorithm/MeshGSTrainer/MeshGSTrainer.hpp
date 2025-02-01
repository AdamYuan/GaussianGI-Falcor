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

namespace Concepts
{
template<typename T>
concept MeshGSTrainSplatAttrib =
    sizeof(T) % sizeof(float) == 0 && alignof(T) == sizeof(float) && requires(const T& t, const ShaderVar& var) { t.bindShaderData(var); };
template<typename T>
concept MeshGSTrainEmptySplatAttrib = std::is_empty_v<T>;
template<typename T>
concept MeshGSTrainSplatChannel = sizeof(T) % sizeof(float) == 0 && alignof(T) == sizeof(float);
template<typename T>
concept MeshGSTrainSplatTexture = requires(const T& ct, const ref<Device>& pDevice, const ShaderVar& var, RenderContext* pRenderContext) {
    { T::create(pDevice, uint2{}) } -> std::convertible_to<T>;
    { ct.bindShaderData(var) } -> std::same_as<void>;
    { ct.bindRsMsShaderData(var) } -> std::same_as<void>;
    { ct.clearUavRsMs(pRenderContext) } -> std::same_as<void>;
    { ct.isCapable(uint2{}) } -> std::convertible_to<bool>;
};
template<typename T>
concept MeshGSTrainSplatRTTexture = requires(const T& ct, const ref<Device>& pDevice, const ShaderVar& var, RenderContext* pRenderContext) {
    { T::create(pDevice, uint2{}) } -> std::convertible_to<T>;
    { T::getBlendStateDesc() } -> std::convertible_to<BlendState::Desc>;
    { ct.bindShaderData(var) } -> std::same_as<void>;
    { ct.clearRtv(pRenderContext) } -> std::same_as<void>;
    { ct.isCapable(uint2{}) } -> std::convertible_to<bool>;
    { ct.getFbo() } -> std::convertible_to<ref<Fbo>>;
};
template<typename T>
concept MeshGSTrainMeshRTTexture = requires(const T& ct, const ref<Device>& pDevice, const ShaderVar& var, RenderContext* pRenderContext) {
    { T::create(pDevice, uint2{}) } -> std::convertible_to<T>;
    { ct.bindShaderData(var) } -> std::same_as<void>;
    { ct.clearRtv(pRenderContext) } -> std::same_as<void>;
    { ct.isCapable(uint2{}) } -> std::convertible_to<bool>;
    { ct.getFbo() } -> std::convertible_to<ref<Fbo>>;
};

template<typename T>
concept MeshGSTrainTrait = requires {
    // requires std::derived_from<T, MeshGSTrainTraitBase<T>>;

    T::kIncludePath;
    requires std::convertible_to<decltype(T::kIncludePath), std::string_view>;

    T::kFloatsPerSplatAttrib;
    requires std::convertible_to<decltype(T::kFloatsPerSplatAttrib), uint>;
    typename T::SplatAttrib;
    requires MeshGSTrainSplatAttrib<typename T::SplatAttrib> || MeshGSTrainEmptySplatAttrib<typename T::SplatAttrib>;
    requires !MeshGSTrainSplatAttrib<typename T::SplatAttrib> ||
                 T::kFloatsPerSplatAttrib == sizeof(typename T::SplatAttrib) / sizeof(float);
    requires !MeshGSTrainEmptySplatAttrib<typename T::SplatAttrib> || T::kFloatsPerSplatAttrib == 0;

    T::kFloatsPerSplatChannel;
    requires std::convertible_to<decltype(T::kFloatsPerSplatChannel), uint>;
    typename T::SplatChannel;
    requires MeshGSTrainSplatChannel<typename T::SplatChannel>;
    requires T::kFloatsPerSplatChannel == sizeof(typename T::SplatChannel) / sizeof(float);

    typename T::SplatTexture;
    requires MeshGSTrainSplatTexture<typename T::SplatTexture>;
    typename T::SplatRTTexture;
    requires MeshGSTrainSplatRTTexture<typename T::SplatRTTexture>;
    typename T::MeshRTTexture;
    requires MeshGSTrainMeshRTTexture<typename T::MeshRTTexture>;
};

} // namespace Concepts
struct MeshGSTrainState
{
    uint iteration = 0, batch = 0;
    float adamBeta1T = 1.0f, adamBeta2T = 1.0f;
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

struct MeshGSTrainSplatGeom
{
    static constexpr uint kFloatCount = 10;

    float4 rotate{};
    float3 mean{};
    float3 scale{};

    void bindShaderData(const ShaderVar& var) const
    {
        var["rotate"] = rotate;
        var["mean"] = mean;
        var["scale"] = scale;
    }
};
static_assert(sizeof(MeshGSTrainSplatGeom) == MeshGSTrainSplatGeom::kFloatCount * sizeof(float));
static_assert(alignof(MeshGSTrainSplatGeom) == sizeof(float));

inline constexpr uint kMeshGSTrainSplatViewGeomFloatCount = 5;

template<typename Derived_T>
struct MeshGSTrainTraitBase
{
    struct SplatWithAttrib
    {
        MeshGSTrainSplatGeom geom;
        typename Derived_T::SplatAttrib attrib{};
        void bindShaderData(const ShaderVar& var) const
        {
            geom.bindShaderData(var["geom"]);
            attrib.bindShaderData(var["attrib"]);
        }
    };
    struct SplatEmptyAttrib
    {
        MeshGSTrainSplatGeom geom;
        void bindShaderData(const ShaderVar& var) const { geom.bindShaderData(var["geom"]); }
    };
    using Splat =
        std::conditional_t<Concepts::MeshGSTrainEmptySplatAttrib<typename Derived_T::SplatAttrib>, SplatEmptyAttrib, SplatWithAttrib>;

    static constexpr uint kFloatsPerSplat = MeshGSTrainSplatGeom::kFloatCount + Derived_T::kFloatsPerSplatAttrib;
    static_assert(sizeof(Splat) == kFloatsPerSplat * sizeof(float));
    static constexpr uint kFloatsPerSplatView = kMeshGSTrainSplatViewGeomFloatCount + Derived_T::kFloatsPerSplatChannel;
    static constexpr uint kFloatsPerSplatAdam = kFloatsPerSplat * 2;

    using SplatSOAUnitTrait = SOAUnitTrait<float, 4>;
    using SplatSOATrait = SOATrait<SplatSOAUnitTrait, kFloatsPerSplat>;
    using SplatViewSOATrait = SOATrait<SplatSOAUnitTrait, kFloatsPerSplatView>;
    using SplatAdamSOATrait = SOATrait<SplatSOAUnitTrait, kFloatsPerSplatAdam>;

    using SplatBuffer = SOABuffer<SplatSOATrait>;
    using SplatBufferInitData = SOABufferInitData<SplatSOATrait>;
    using SplatViewBuffer = SOABuffer<SplatViewSOATrait>;
    using SplatAdamBuffer = SOABuffer<SplatAdamSOATrait>;

    struct Desc
    {
        uint maxSplatCount;
        uint2 resolution;
        uint batchSize;
        Splat learnRate;
    };

    struct Resource
    {
        typename Derived_T::SplatRTTexture splatRT;
        // typename Derived_T::MeshRTTexture meshRT;
        typename Derived_T::SplatTexture splatDLossTex, splatTmpTex;
        SplatBuffer splatBuf, splatDLossBuf;
        SplatAdamBuffer splatAdamBuf;
        SplatViewBuffer splatViewBuf, splatViewDLossBuf;
        ref<Buffer> pSplatViewSplatIDBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer, pSplatViewAxisBuffer;
        ref<Buffer> pSplatViewDrawArgBuffer, pSplatViewDispatchArgBuffer;

        static Resource create(
            const ref<Device>& pDevice,
            uint splatCount,
            uint2 resolution,
            const SplatBufferInitData& splatInitData = {}
        );
        static Resource create(const ref<Device>& pDevice, const Desc& desc, const SplatBufferInitData& splatInitData = {})
        {
            return create(pDevice, desc.maxSplatCount, desc.resolution, splatInitData);
        }
        static SplatBuffer createSplatBuffer(const ref<Device>& pDevice, uint splatCount, const SplatBufferInitData& splatInitData = {});
        static SplatAdamBuffer createSplatAdamBuffer(const ref<Device>& pDevice, uint splatCount);
        static SplatViewBuffer createSplatViewBuffer(const ref<Device>& pDevice, uint splatViewCount);
        bool isCapable(uint splatCount, uint2 resolution) const;
        bool isCapable(const Desc& desc) const { return isCapable(desc.maxSplatCount, desc.resolution); }
    };

    struct Data
    {
        typename Derived_T::MeshRTTexture meshRT;
        MeshGSTrainCamera camera;
    };
};

namespace Concepts
{
template<typename T, MeshGSTrainTrait Trait_T>
concept MeshGSTrainDataset = requires(const T& ct, T& t, typename Trait_T::Data& data) {
    { ct.getResolution() } -> std::convertible_to<uint2>;
    ct.generate(std::declval<RenderContext*>(), data, bool{} /* generateCamera */);
};
} // namespace Concepts

template<Concepts::MeshGSTrainTrait Trait_T>
class MeshGSTrainer
{
private:
    typename Trait_T::Desc mDesc{};
    ref<ComputePass> mpForwardViewPass, mpBackwardViewPass, mpBackwardCmdPass, mpOptimizePass, mpLossPass;
    ref<RasterPass> mpForwardDrawPass, mpBackwardDrawPass;

    void reset(RenderContext* pRenderContext, const typename Trait_T::Resource& resource) const;
    void forward(
        RenderContext* pRenderContext,
        const typename Trait_T::Resource& resource,
        const MeshGSTrainCamera& camera,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const;
    void loss(RenderContext* pRenderContext, const typename Trait_T::Resource& resource, const typename Trait_T::Data& data) const;
    void backward(RenderContext* pRenderContext, const typename Trait_T::Resource& resource, const MeshGSTrainCamera& camera) const;
    void optimize(const MeshGSTrainState& state, RenderContext* pRenderContext, const typename Trait_T::Resource& resource) const;

public:
    MeshGSTrainer() = default;
    MeshGSTrainer(const ref<Device>& pDevice, const typename Trait_T::Desc& desc);
    static DeviceSortDesc getSortDesc() { return DeviceSortDesc({DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload}); }

    const auto& getDesc() const { return mDesc; }

    void iterate(
        MeshGSTrainState& state,
        RenderContext* pRenderContext,
        const typename Trait_T::Resource& resource,
        const typename Trait_T::Data& data,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const;

    void inference(
        RenderContext* pRenderContext,
        const typename Trait_T::Resource& resource,
        const MeshGSTrainCamera& camera,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const
    {
        forward(pRenderContext, resource, camera, sorter, sortResource);
    }
};

} // namespace GSGI

#endif // GSGI_MESHGSTRAINER_HPP
