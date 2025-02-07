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
concept MeshGSTrainSplatAttrib = (std::is_empty_v<T> || sizeof(T) % sizeof(float) == 0 && alignof(T) == sizeof(float)) &&
                                 requires(const T& t, const ShaderVar& var) { t.bindShaderData(var); };
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
    requires MeshGSTrainSplatAttrib<typename T::SplatAttrib>;
    requires T::kFloatsPerSplatAttrib == sizeof(typename T::SplatAttrib) / sizeof(float);

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

template<Concepts::MeshGSTrainTrait Trait_T>
class MeshGSTrainer
{
public:
    using Trait = Trait_T;

    static void addDefine(DefineList& defineList) { defineList.add("TRAIT_INCLUDE_PATH", fmt::format("\"{}\"", Trait_T::kIncludePath)); }
    struct Splat
    {
        MeshGSTrainSplatGeom geom;
#ifdef _MSC_VER
        [[msvc::no_unique_address]]
#else
        [[no_unique_address]]
#endif
        typename Trait_T::SplatAttrib attrib{};

        void bindShaderData(const ShaderVar& var) const
        {
            geom.bindShaderData(var["geom"]);
            attrib.bindShaderData(var["attrib"]);
        }
    };

    static constexpr uint kFloatsPerSplat = MeshGSTrainSplatGeom::kFloatCount + Trait_T::kFloatsPerSplatAttrib;
    static_assert(sizeof(Splat) == kFloatsPerSplat * sizeof(float));
    static constexpr uint kFloatsPerSplatView = kMeshGSTrainSplatViewGeomFloatCount + Trait_T::kFloatsPerSplatChannel;
    static constexpr uint kFloatsPerSplatAdam = kFloatsPerSplat * 2;

    using SplatSOAUnitTrait = SOAUnitTrait<float, 4>;
    using SplatSOATrait = SOATrait<SplatSOAUnitTrait, kFloatsPerSplat>;
    using SplatViewSOATrait = SOATrait<SplatSOAUnitTrait, kFloatsPerSplatView>;
    using SplatAdamSOATrait = SOATrait<SplatSOAUnitTrait, kFloatsPerSplatAdam>;

    using SplatBuffer = SOABuffer<SplatSOATrait>;
    using SplatBufferData = SOABufferData<SplatSOATrait>;
    using SplatViewBuffer = SOABuffer<SplatViewSOATrait>;
    using SplatAdamBuffer = SOABuffer<SplatAdamSOATrait>;

    struct Desc
    {
        uint maxSplatCount{};
        uint2 resolution{};
        uint batchSize{};
        Splat learnRate;
    };

    struct Resource
    {
        typename Trait_T::SplatRTTexture splatRT;
        // typename Derived_T::MeshRTTexture meshRT;
        typename Trait_T::SplatTexture splatDLossTex, splatTmpTex;
        SplatBuffer splatBuf, splatDLossBuf;
        SplatAdamBuffer splatAdamBuf;
        SplatViewBuffer splatViewBuf, splatViewDLossBuf;
        ref<Buffer> pSplatViewSplatIDBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer, pSplatViewAxisBuffer;
        ref<Buffer> pSplatViewDrawArgBuffer, pSplatViewDispatchArgBuffer;

        static Resource create(
            const ref<Device>& pDevice,
            uint splatCount,
            uint2 resolution,
            const SplatBufferData& splatInitData = {}
        );
        static Resource create(const ref<Device>& pDevice, const Desc& desc, const SplatBufferData& splatInitData = {})
        {
            return create(pDevice, desc.maxSplatCount, desc.resolution, splatInitData);
        }
        static SplatBuffer createSplatBuffer(const ref<Device>& pDevice, uint splatCount, const SplatBufferData& splatInitData = {});
        static SplatAdamBuffer createSplatAdamBuffer(const ref<Device>& pDevice, uint splatCount);
        static SplatViewBuffer createSplatViewBuffer(const ref<Device>& pDevice, uint splatViewCount);
        bool isCapable(uint splatCount, uint2 resolution) const;
        bool isCapable(const Desc& desc) const { return isCapable(desc.maxSplatCount, desc.resolution); }
    };

    struct Data
    {
        typename Trait_T::MeshRTTexture meshRT;
        MeshGSTrainCamera camera;
    };

private:
    Desc mDesc{};
    ref<ComputePass> mpForwardViewPass, mpBackwardViewPass, mpBackwardCmdPass, mpOptimizePass, mpLossPass;
    ref<RasterPass> mpForwardDrawPass, mpBackwardDrawPass;

    void reset(RenderContext* pRenderContext, const Resource& resource) const;
    void forward(
        RenderContext* pRenderContext,
        const Resource& resource,
        const MeshGSTrainCamera& camera,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const;
    void loss(RenderContext* pRenderContext, const Resource& resource, const Data& data) const;
    void backward(RenderContext* pRenderContext, const Resource& resource, const MeshGSTrainCamera& camera) const;
    void optimize(const MeshGSTrainState& state, RenderContext* pRenderContext, const Resource& resource) const;

public:
    MeshGSTrainer() = default;
    MeshGSTrainer(const ref<Device>& pDevice, const Desc& desc);
    static DeviceSortDesc getSortDesc() { return DeviceSortDesc({DeviceSortBufferType::kKey32, DeviceSortBufferType::kPayload}); }

    const auto& getDesc() const { return mDesc; }

    void iterate(
        MeshGSTrainState& state,
        RenderContext* pRenderContext,
        const Resource& resource,
        const Data& data,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const;

    void inference(
        RenderContext* pRenderContext,
        const Resource& resource,
        const MeshGSTrainCamera& camera,
        const DeviceSorter<DeviceSortDispatchType::kIndirect>& sorter,
        const DeviceSortResource<DeviceSortDispatchType::kIndirect>& sortResource
    ) const
    {
        forward(pRenderContext, resource, camera, sorter, sortResource);
    }
};

namespace Concepts
{
template<typename T, typename Trait_T>
concept MeshGSTrainDataset = requires(const T& ct, T& t, typename MeshGSTrainer<Trait_T>::Data& data) {
    { t.generate(std::declval<RenderContext*>(), data, uint2{} /* resolution */, bool{} /* generateCamera */) } -> std::same_as<void>;
};
} // namespace Concepts

} // namespace GSGI

#endif // GSGI_MESHGSTRAINER_HPP
