//
// Created by adamyuan on 1/12/25.
//

#include "GMeshGSTrainDataset.hpp"

#include "../Util/ShaderUtil.hpp"
#include <Utils/Math/MathConstants.slangh>

#include "../Algorithm/MeshGSTrainer/Trait/Depth.hpp"
#include "../Algorithm/MeshGSTrainer/Trait/DepthAlbedo.hpp"

namespace GSGI
{

namespace
{
MeshGSTrainCamera randomCamera(const AABB& bound, float eyeExtent, uint2 resolution, auto& randGen)
{
    MeshGSTrainCamera trainCamera;

    float aspectRatio = float(resolution.x) / float(resolution.y);
    auto boundExtent = bound.extent();
    auto boundDiag = math::sqrt(math::dot(boundExtent, boundExtent));
    trainCamera.farZ = boundDiag * 2.0f;
    trainCamera.nearZ = boundDiag * 0.01f;
    trainCamera.projMat = math::perspective(float(M_PI / 3.0), aspectRatio, trainCamera.nearZ, trainCamera.farZ);

    auto boundHalfExtent = 0.5f * boundExtent;
    auto boundCenter = bound.center();

    float3 eyePos, eyeLookAt;
    eyeExtent = math::max(1.0f, eyeExtent);
    do
    {
        for (int i = 0; i < 3; ++i)
        {
            auto distr = std::uniform_real_distribution<float>{-boundHalfExtent[i], boundHalfExtent[i]};
            eyePos[i] = boundCenter[i] + distr(randGen) * eyeExtent;
            eyeLookAt[i] = boundCenter[i] + distr(randGen);
        }
    } while (math::all(eyeLookAt.xz() == eyePos.xz()));
    trainCamera.viewMat = math::matrixFromLookAt(eyePos, eyeLookAt, float3{0.0f, 1.0f, 0.0f});

    return trainCamera;
}
} // namespace

template<Concepts::MeshGSTrainTrait Trait_T>
void GMeshGSTrainDataset<Trait_T>::generate(
    RenderContext* pRenderContext,
    typename MeshGSTrainer<Trait_T>::Data& data,
    uint2 resolution,
    bool generateCamera
)
{
    const auto& pDevice = pRenderContext->getDevice();

    if (!data.meshRT.isCapable(resolution))
        data.meshRT = Trait_T::MeshRTTexture::create(pDevice, resolution);

    if (generateCamera)
        data.camera = randomCamera(pMesh->getBound(), config.eyeExtent, resolution, randGen);

    static ref<RasterPass> spPass = [pDevice]
    {
        ProgramDesc desc;
        desc.addShaderLibrary("GaussianGI/Scene/GMeshGSTrainDataset.3d.slang").vsEntry("vsMain").gsEntry("gsMain").psEntry("psMain");
        DefineList defList;
        MeshGSTrainer<Trait_T>::addDefine(defList);
        if constexpr (std::same_as<Trait_T, MeshGSTrainDepthTrait>)
            defList.add("TRAIT", "DEPTH_TRAIT");
        else if constexpr (std::same_as<Trait_T, MeshGSTrainDepthAlbedoTrait>)
            defList.add("TRAIT", "DEPTH_ALBEDO_TRAIT");
        else
            static_assert(false, "Not implemented");
        auto pPass = RasterPass::create(pDevice, desc, defList);
        pPass->getState()->setRasterizerState(GMesh::getRasterizerState());
        return pPass;
    }();

    data.meshRT.clearRtv(pRenderContext);
    spPass->getState()->setFbo(data.meshRT.getFbo());
    auto [prog, var] = getShaderProgVar(spPass);
    var["gSampler"] = pDevice->getDefaultSampler();
    data.camera.bindShaderData(var["gCamera"]);
    pMesh->draw(pRenderContext, spPass, var["gMeshRasterData"]);
}

template struct GMeshGSTrainDataset<MeshGSTrainDepthTrait>;
static_assert(Concepts::MeshGSTrainDataset<GMeshGSTrainDataset<MeshGSTrainDepthTrait>, MeshGSTrainDepthTrait>);
template struct GMeshGSTrainDataset<MeshGSTrainDepthAlbedoTrait>;
static_assert(Concepts::MeshGSTrainDataset<GMeshGSTrainDataset<MeshGSTrainDepthAlbedoTrait>, MeshGSTrainDepthAlbedoTrait>);

} // namespace GSGI