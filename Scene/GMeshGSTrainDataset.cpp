//
// Created by adamyuan on 1/12/25.
//

#include "GMeshGSTrainDataset.hpp"

#include "../Util/ShaderUtil.hpp"

namespace GSGI
{

template<MeshGSTrainType TrainType_V>
void GMeshGSTrainDataset<TrainType_V>::generate(
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
    mesh.draw(pRenderContext, spPass, var["gMeshRasterData"]);
}

template struct GMeshGSTrainDataset<MeshGSTrainType::kDepth>;

} // namespace GSGI