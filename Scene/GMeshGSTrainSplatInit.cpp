//
// Created by adamyuan on 2/5/25.
//

#include "GMeshGSTrainSplatInit.hpp"

#include "../Algorithm/MeshGSOptimize.hpp"
#include "../Algorithm/MeshSample.hpp"
#include "GMeshView.hpp"
#include <boost/random/sobol.hpp>
#include <ranges>

#include "../Algorithm/MeshGSTrainer/Trait/Depth.hpp"
#include "../Algorithm/MeshGSTrainer/Trait/DepthAlbedo.hpp"
#include "../Util/ShaderUtil.hpp"

namespace GSGI
{

namespace
{

MeshSample::Result samplePoints(const GMeshView& meshView, uint count)
{
    return MeshSample::sample(
        meshView,
        [sobolEngine = boost::random::sobol_engine<uint32_t, 32>{4}] mutable
        {
            uint2 u2;
            float2 f2;
            u2.x = sobolEngine();
            u2.y = sobolEngine();
            f2.x = float(sobolEngine()) / 4294967296.0f;
            f2.y = float(sobolEngine()) / 4294967296.0f;
            return std::tuple{u2, f2};
        },
        count
    );
}

} // namespace

template<Concepts::MeshGSTrainTrait Trait_T>
void GMeshGSTrainSplatInit<Trait_T>::initialize(
    RenderContext* pRenderContext,
    const typename MeshGSTrainer<Trait_T>::Resource& resource,
    uint splatCount,
    float initialScaleCoef
) const
{
    auto view = GMeshView{pMesh};
    auto sampleResult = samplePoints(view, splatCount);

    const auto& pDevice = pRenderContext->getDevice();

    static ref<ComputePass> spPass = [pDevice]
    {
        DefineList defList;
        MeshGSTrainer<Trait_T>::addDefine(defList);
        if constexpr (std::same_as<Trait_T, MeshGSTrainDepthTrait>)
            defList.add("TRAIT", "DEPTH_TRAIT");
        else if constexpr (std::same_as<Trait_T, MeshGSTrainDepthAlbedoTrait>)
            defList.add("TRAIT", "DEPTH_ALBEDO_TRAIT");
        else
            static_assert(false, "Not implemented");
        auto pPass = ComputePass::create(pDevice, "GaussianGI/Scene/GMeshGSTrainSplatInit.cs.slang", "csMain", defList);
        return pPass;
    }();

    // Upload temporal meshPoint data
    static_assert(sizeof(MeshPoint) == 3 * sizeof(uint32_t));
    static_assert(offsetof(MeshPoint, primitiveID) == 0);
    auto pMeshPointBuffer = pDevice->createStructuredBuffer(
        sizeof(MeshPoint), splatCount, ResourceBindFlags::ShaderResource, MemoryType::DeviceLocal, sampleResult.points.data()
    );

    // Execute shader
    auto [prog, var] = getShaderProgVar(spPass);
    pMesh->bindShaderData(var["gGMeshData"]);
    resource.splatBuf.bindShaderData(var["gSplatBuffer"]);
    var["gMeshPoints"] = pMeshPointBuffer;
    var["gSplatCount"] = splatCount;
    var["gSplatScale"] = (float)MeshGSOptimize::getInitialScale(sampleResult.totalArea, splatCount, initialScaleCoef);
    var["gSampler"] = pDevice->getDefaultSampler();

    spPass->execute(pRenderContext, splatCount, 1, 1);
}

template struct GMeshGSTrainSplatInit<MeshGSTrainDepthTrait>;
template struct GMeshGSTrainSplatInit<MeshGSTrainDepthAlbedoTrait>;

} // namespace GSGI