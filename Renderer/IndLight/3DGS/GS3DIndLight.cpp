//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include "../../../Algorithm/MeshSample.hpp"
#include "../../../Scene/GMeshView.hpp"
#include "GS3DIndLightMisc.slangh"
#include "../../../Util/ShaderUtil.hpp"

namespace GSGI
{

GS3DIndLight::GS3DIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    mpPointVertexLayout = []
    {
        auto vertexBufferLayout = VertexBufferLayout::create();
        vertexBufferLayout->addElement(GS3D_VERTEX_PRIMID_NAME, 0, ResourceFormat::R32Uint, 1, GS3D_VERTEX_PRIMID_LOC);
        vertexBufferLayout->addElement(GS3D_VERTEX_BARY_NAME, sizeof(uint32_t), ResourceFormat::RG32Float, 1, GS3D_VERTEX_BARY_LOC);
        static_assert(sizeof(MeshSample) == 12);
        static_assert(offsetof(MeshSample, primitiveID) == 0);
        static_assert(offsetof(MeshSample, barycentrics) == sizeof(uint32_t));
        auto vertexLayout = VertexLayout::create();
        vertexLayout->addBufferLayout(0, std::move(vertexBufferLayout));
        return vertexLayout;
    }();
    mpPointPass = RasterPass::create(getDevice(), "GaussianGI/Renderer/IndLight/3DGS/GS3DIndLightMiscPoint.3d.slang", "vsMain", "psMain");
}

void GS3DIndLight::update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
{
    mpStaticScene = pDefaultStaticScene;
    if (isSceneChanged)
    {
        std::vector<MeshSample> meshPoints;
        for (const auto& pMesh : pDefaultStaticScene->getMeshes())
        {
            auto sampler = MeshSamplerDefault<std::mt19937>{};
            auto view = GMeshView::make(pMesh);
            auto meshSamples = MeshSample::sample(view, sampler, mConfig.splatsPerMesh);
            meshPoints.insert(meshPoints.end(), meshSamples.begin(), meshSamples.end());
        }

        mpPointBuffer = getDevice()->createStructuredBuffer(
            sizeof(MeshSample), meshPoints.size(), ResourceBindFlags::Vertex, MemoryType::DeviceLocal, meshPoints.data()
        );
        mpPointVao = Vao::create(Vao::Topology::PointList, mpPointVertexLayout, {mpPointBuffer});
        mpPointPass->getState()->setVao(mpPointVao);
    }
}

void GS3DIndLight::draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture) {}

void GS3DIndLight::drawMisc(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    pRenderContext->clearDsv(pTargetFbo->getDepthStencilView().get(), 1.0f, 0, true, false);
    pRenderContext->clearTexture(pTargetFbo->getColorTexture(0).get(), float4{});

    auto [prog, var] = getShaderProgVar(mpPointPass);
    mpStaticScene->bindRootShaderData(var);
    mpPointPass->getState()->setFbo(pTargetFbo);

    for (uint meshID = 0; meshID < mpStaticScene->getMeshCount(); ++meshID)
    {
        const auto& meshInfo = mpStaticScene->getMeshInfos()[meshID];
        pRenderContext->drawInstanced(
            mpPointPass->getState().get(),
            mpPointPass->getVars().get(),
            mConfig.splatsPerMesh,
            meshInfo.instanceCount,
            mConfig.splatsPerMesh * meshID,
            meshInfo.firstInstance
        );
    }
}

void GS3DIndLight::renderUIImpl(Gui::Widgets& widget) {}

} // namespace GSGI