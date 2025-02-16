//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include "GS3DIndLightSplat.hpp"
#include "GS3DIndLightAlgo.hpp"
#include "../../../Algorithm/MeshVHBVH.hpp"
#include "../../../Util/ShaderUtil.hpp"
#include "../../../Util/TextureUtil.hpp"

namespace GSGI
{

namespace
{
constexpr uint32_t kDefaultSplatsPerMesh = 65536;
}

void GS3DIndLight::updateDrawResource(const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture)
{
    if (!mDrawResource.pCullPass)
        mDrawResource.pCullPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/3DGS/GS3DIndLightCull.cs.slang", "csMain");

    if (!mDrawResource.pDrawPass)
    {
        ProgramDesc splatDrawDesc;
        splatDrawDesc.addShaderLibrary("GaussianGI/Renderer/IndLight/3DGS/GS3DIndLightDraw.3d.slang")
            .vsEntry("vsMain")
            .gsEntry("gsMain")
            .psEntry("psMain");
        mDrawResource.pDrawPass = RasterPass::create(getDevice(), splatDrawDesc);
        mDrawResource.pDrawPass->getState()->setVao(Vao::create(Vao::Topology::PointList));

        DepthStencilState::Desc splatDepthDesc;
        splatDepthDesc.setDepthEnabled(false);
        splatDepthDesc.setDepthWriteMask(false);
        BlendState::Desc splatBlendDesc;
        splatBlendDesc.setRtBlend(0, true).setRtParams(
            0,
            BlendState::BlendOp::Add,
            BlendState::BlendOp::Add,
            BlendState::BlendFunc::One,
            BlendState::BlendFunc::One,
            BlendState::BlendFunc::One,
            BlendState::BlendFunc::One
        );
        RasterizerState::Desc splatRasterDesc;
        splatRasterDesc.setCullMode(RasterizerState::CullMode::None);
        mDrawResource.pDrawPass->getState()->setRasterizerState(RasterizerState::create(splatRasterDesc));
        mDrawResource.pDrawPass->getState()->setBlendState(BlendState::create(splatBlendDesc));
        mDrawResource.pDrawPass->getState()->setDepthStencilState(DepthStencilState::create(splatDepthDesc));
    }

    if (!mDrawResource.pBlendPass)
        mDrawResource.pBlendPass =
            ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/3DGS/GS3DIndLightBlend.cs.slang", "csMain");

    if (!mDrawResource.pSplatIDBuffer || mDrawResource.pSplatIDBuffer->getElementCount() != mSplatCount)
        mDrawResource.pSplatIDBuffer = getDevice()->createStructuredBuffer(
            sizeof(uint32_t), mSplatCount, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
        );

    if (!mDrawResource.pSplatDrawArgBuffer)
    {
        DrawArguments splatDrawArgs = {
            .VertexCountPerInstance = 1,
            .InstanceCount = 0,
            .StartVertexLocation = 0,
            .StartInstanceLocation = 0,
        };
        mDrawResource.pSplatDrawArgBuffer = getDevice()->createStructuredBuffer(
            sizeof(DrawArguments),
            1,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::IndirectArg,
            MemoryType::DeviceLocal,
            &splatDrawArgs
        );
        static_assert(sizeof(DrawArguments) == 4 * sizeof(uint32_t));
    }

    uint2 resolution = getTextureResolution2(pIndirectTexture);
    updateTextureSize(
        mDrawResource.pSplatFbo,
        resolution,
        [&](uint width, uint height)
        {
            return Fbo::create(
                getDevice(),
                {pIndirectTexture},
                nullptr // args.pVBuffer->getDepthStencilTexture()
            );
        }
    );
}

GS3DIndLight::GS3DIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    mpMiscRenderer = make_ref<GS3DMiscRenderer>(getDevice());
}

void GS3DIndLight::update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
{
    mpStaticScene = pDefaultStaticScene;
    if (isSceneChanged)
    {
        std::vector<GS3DIndLightSplat> splats;
        std::vector<uint32_t> meshFirstSplatIdx;
        meshFirstSplatIdx.reserve(mpStaticScene->getMeshCount());
        for (const auto& pMesh : mpStaticScene->getMeshes())
        {
            auto meshView = GMeshView{pMesh};
            MeshBVH<AABB> meshBVH;
            auto meshSplats = GS3DIndLightSplat::loadMesh(pMesh);
            if (meshSplats.empty())
            {
                if (meshBVH.isEmpty())
                    meshBVH = MeshBVH<AABB>::build<MeshVHBVHBuilder>(meshView);
                meshSplats = GS3DIndLightAlgo::getSplatsFromMeshFallback(meshView, meshBVH, kDefaultSplatsPerMesh);
                GS3DIndLightSplat::persistMesh(pMesh, meshSplats);
            }
            meshFirstSplatIdx.push_back(splats.size());
            splats.insert(splats.end(), meshSplats.begin(), meshSplats.end());

            /* {
                if (meshBVH.isEmpty())
                    meshBVH = MeshBVH<AABB>::build<MeshVHBVHBuilder>(meshView);

                std::vector<std::vector<uint32_t>> primSplatIDs =
                    GS3DIndLightAlgo::getPrimitiveIntersectedSplatIDs(meshView, meshBVH, meshSplats);

                uint32_t greater16Cnt = 0, greater8Cnt = 0, equal0Cnt = 0;
                for (uint i = 0; i < pMesh->getPrimitiveCount(); ++i)
                {
                    if (primSplatIDs[i].size() > 16)
                        ++greater16Cnt;
                    if (primSplatIDs[i].size() > 8)
                        ++greater8Cnt;
                    if (primSplatIDs[i].empty())
                        ++equal0Cnt;
                }
                fmt::println(">16: {}, >8: {}, =0: {}", greater16Cnt, greater8Cnt, equal0Cnt);
            } */
        }

        std::vector<uint32_t> splatDescs;

        for (uint32_t instanceID = 0; instanceID < mpStaticScene->getInstanceCount(); ++instanceID)
        {
            const auto& instanceInfo = mpStaticScene->getInstanceInfos()[instanceID];
            uint32_t meshID = instanceInfo.meshID;
            uint32_t firstSplatIdx = meshFirstSplatIdx[meshID];
            uint32_t splatCount =
                (meshID == mpStaticScene->getMeshCount() - 1 ? splats.size() : meshFirstSplatIdx[instanceInfo.meshID + 1]) - firstSplatIdx;

            for (uint32_t splatOfst = 0; splatOfst < splatCount; ++splatOfst)
                splatDescs.push_back((firstSplatIdx + splatOfst) | (instanceID << 24u));

            static_assert(GStaticScene::kMaxInstanceCount <= 256);
        }

        mpSplatBuffer = getDevice()->createStructuredBuffer(
            sizeof(GS3DIndLightSplat), //
            splats.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splats.data()
        );
        mpSplatDescBuffer = getDevice()->createStructuredBuffer(
            sizeof(uint32_t), //
            splatDescs.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splatDescs.data()
        );
        mSplatCount = splatDescs.size();
    }
}

void GS3DIndLight::draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture)
{
    updateDrawResource(args, pIndirectTexture);

    uint2 resolution = getTextureResolution2(pIndirectTexture);
    auto resolutionFloat = float2(resolution);

    GS3DIndLightInstancedSplatBuffer instancedSplatBuffer = {
        .pSplatBuffer = mpSplatBuffer,
        .pSplatDescBuffer = mpSplatDescBuffer,
        .splatCount = mSplatCount,
    };

    // Reset
    static_assert(offsetof(DrawArguments, InstanceCount) == sizeof(uint32_t));
    mDrawResource.pSplatDrawArgBuffer->setElement<uint32_t>(offsetof(DrawArguments, InstanceCount) / sizeof(uint32_t), 0u);

    // Splat Cull Pass
    {
        FALCOR_PROFILE(pRenderContext, "cull");
        auto [prog, var] = getShaderProgVar(mDrawResource.pCullPass);
        mpStaticScene->bindRootShaderData(var);
        instancedSplatBuffer.bindShaderData(var["gSplats"]);
        var["gResolution"] = resolutionFloat;

        var["gSplatDrawArgs"] = mDrawResource.pSplatDrawArgBuffer;
        var["gSplatIDs"] = mDrawResource.pSplatIDBuffer;

        mDrawResource.pCullPass->execute(pRenderContext, mSplatCount, 1, 1);
    }

    // Sort
    /* {
        FALCOR_PROFILE(pRenderContext, "sortSplat");
        mDrawResource.splatViewSorter.dispatch(
            pRenderContext,
            {mDrawResource.pSplatViewSortKeyBuffer, mDrawResource.pSplatViewSortPayloadBuffer},
            mDrawResource.pSplatViewDrawArgBuffer,
            offsetof(DrawArguments, InstanceCount),
            mDrawResource.splatViewSortResource
        );
    } */

    // Splat Draw Pass
    {
        FALCOR_PROFILE(pRenderContext, "draw");
        pRenderContext->clearRtv(pIndirectTexture->getRTV().get(), float4{});
        auto [prog, var] = getShaderProgVar(mDrawResource.pDrawPass);
        mpStaticScene->bindRootShaderData(var);
        instancedSplatBuffer.bindShaderData(var["gSplats"]);
        var["gSplatIDs"] = mDrawResource.pSplatIDBuffer;
        var["gResolution"] = resolutionFloat;
        args.pVBuffer->bindShaderData(var["gGVBuffer"]);

        mDrawResource.pDrawPass->getState()->setFbo(mDrawResource.pSplatFbo);
        pRenderContext->drawIndirect(
            mDrawResource.pDrawPass->getState().get(),
            mDrawResource.pDrawPass->getVars().get(),
            1,
            mDrawResource.pSplatDrawArgBuffer.get(),
            0,
            nullptr,
            0
        );
    }

    // OIT Blend Pass
    {
        FALCOR_PROFILE(pRenderContext, "blend");
        auto [prog, var] = getShaderProgVar(mDrawResource.pBlendPass);
        var["gResolution"] = resolution;
        var["gIndirect"] = pIndirectTexture;

        mDrawResource.pBlendPass->execute(pRenderContext, resolution.x, resolution.y, 1);
    }
}

void GS3DIndLight::drawMisc(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo)
{
    mpMiscRenderer->draw(
        pRenderContext,
        pTargetFbo,
        {
            .pStaticScene = mpStaticScene,
            .pSplatBuffer = mpSplatBuffer,
            .pSplatDescBuffer = mpSplatDescBuffer,
            .splatCount = mSplatCount,
        }
    );
}

void GS3DIndLight::renderUIImpl(Gui::Widgets& widget)
{
    if (auto g = widget.group("Misc", true))
        mpMiscRenderer->renderUI(g);

    /* auto result = MeshClosestPoint::query(
        GMeshView{mpStaticScene->getMeshes()[0]}, meshBvh, mpStaticScene->getScene()->getCamera()->getPosition(), 1.0f
    );
    widget.text(fmt::format("dist = {}, id = {}", math::sqrt(result.dist2), result.optPrimitiveID)); */
}

} // namespace GSGI