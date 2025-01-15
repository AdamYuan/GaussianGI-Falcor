//
// Created by adamyuan on 1/12/25.
//

#include "GMeshGSTrainDataset.hpp"

#include "../Util/ShaderUtil.hpp"
#include <Utils/Math/MathConstants.slangh>

namespace GSGI
{

template<MeshGSTrainType TrainType_V, typename RandGen_T>
MeshGSTrainCamera GMeshGSTrainDataset<TrainType_V, RandGen_T>::nextCamera(const MeshGSTrainMeshRT<TrainType_V>& rt)
{
    MeshGSTrainCamera trainCamera;

    auto boundExtent = pMesh->getBound().extent();
    auto boundDiag = math::sqrt(math::dot(boundExtent, boundExtent));
    trainCamera.farZ = boundDiag * 2.0f;
    trainCamera.nearZ = boundDiag * 0.01f;
    trainCamera.projMat = math::perspective(float(M_PI / 3.0), rt.getAspectRatio(), trainCamera.nearZ, trainCamera.farZ);

    auto boundHalfExtent = 0.5f * boundExtent;
    auto boundCenter = pMesh->getBound().center();

    float3 eyePos, eyeLookAt;
    float eyeExtent = math::max(1.0f, config.eyeExtent);
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

template<MeshGSTrainType TrainType_V, typename RandGen_T>
void GMeshGSTrainDataset<TrainType_V, RandGen_T>::draw(
    RenderContext* pRenderContext,
    const MeshGSTrainMeshRT<TrainType_V>& rt,
    const MeshGSTrainCamera& camera
) const
{
    const auto& pDevice = pRenderContext->getDevice();
    static ref<RasterPass> spPass = [pDevice]
    {
        ProgramDesc desc;
        desc.addShaderLibrary("GaussianGI/Scene/GMeshGSTrainDataset.3d.slang").vsEntry("vsMain").gsEntry("gsMain").psEntry("psMain");
        auto pPass = RasterPass::create(pDevice, desc);
        pPass->getState()->setRasterizerState(GMesh::getRasterizerState());
        return pPass;
    }();
    auto [prog, var] = getShaderProgVar(spPass);
    var["gSampler"] = pDevice->getDefaultSampler();
    camera.bindShaderData(var["gCamera"]);
    spPass->getState()->setFbo(rt.pFbo);
    pMesh->draw(pRenderContext, spPass, var["gMeshRasterData"]);
}

template struct GMeshGSTrainDataset<MeshGSTrainType::kDepth>;
static_assert(Concepts::MeshGSTrainDataset<GMeshGSTrainDataset<MeshGSTrainType::kDepth>, MeshGSTrainType::kDepth>);

} // namespace GSGI