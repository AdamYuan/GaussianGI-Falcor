//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include "GS3DIndLightSplat.hpp"
#include "GS3DIndLightAlgo.hpp"
#include "../../../Algorithm/GS3DBound.hpp"
#include "../../../Algorithm/MeshVHBVH.hpp"
#include "../../../Algorithm/Icosahedron.hpp"
#include "../../../Util/BLASUtil.hpp"
#include "../../../Util/ShaderUtil.hpp"
#include "../../../Util/TextureUtil.hpp"
#include "../../../Util/TLASUtil.hpp"

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

void GS3DIndLight::onSceneChanged()
{
    std::vector<GS3DIndLightSplat> splats;
    std::vector<std::array<float3, Icosahedron::kVertexCount>> splatIcosahedronVertices;
    std::vector<std::array<uint3, Icosahedron::kTriangleCount>> splatIcosahedronIndices;

    std::vector<uint32_t> meshFirstSplatIdx;
    meshFirstSplatIdx.reserve(mpStaticScene->getMeshCount());

    for (const auto& pMesh : mpStaticScene->getMeshes())
    {
        auto meshView = GMeshView{pMesh};
        MeshBVH<AABB> meshBVH;
        auto meshSplats = [&pMesh, &meshBVH, &meshView]
        {
            auto meshSplats = GS3DIndLightSplat::loadMesh(pMesh);
            if (meshSplats.empty())
            {
                if (meshBVH.isEmpty())
                    meshBVH = MeshBVH<AABB>::build<MeshVHBVHBuilder>(meshView);
                meshSplats = GS3DIndLightAlgo::getSplatsFromMeshFallback(meshView, meshBVH, kDefaultSplatsPerMesh);
                GS3DIndLightSplat::persistMesh(pMesh, meshSplats);
            }
            return meshSplats;
        }();

        meshFirstSplatIdx.push_back(splats.size());

        // For splat buffer
        splats.insert(splats.end(), meshSplats.begin(), meshSplats.end());

        // For splat BLAS & TLAS
        for (std::size_t meshSplatID = 0; meshSplatID < meshSplats.size(); ++meshSplatID)
        {
            const auto& splat = meshSplats[meshSplatID];

            float3x3 splatRotMat = math::matrixFromQuat(quatf(splat.rotate.x, splat.rotate.y, splat.rotate.z, splat.rotate.w));

            std::array<float3, Icosahedron::kVertexCount> vertices = Icosahedron::kVertices;
            for (auto& vertex : vertices)
            {
                vertex /= Icosahedron::kFaceDist;
                vertex *= float3(splat.scale) * GS3DBound::kSqrt2Log100;
                vertex = math::mul(splatRotMat, vertex);
                vertex += splat.mean;
            }
            std::array<uint3, Icosahedron::kTriangleCount> indices = Icosahedron::kTriangles;
            for (uint32_t indexOffset = meshSplatID * Icosahedron::kVertexCount; auto& index : indices)
            {
                index.x += indexOffset;
                index.y += indexOffset;
                index.z += indexOffset;
            }

            splatIcosahedronVertices.push_back(vertices);
            splatIcosahedronIndices.push_back(indices);
        }
    }

    const auto getMeshSplatCount = [&](uint32_t meshID)
    { return (meshID == mpStaticScene->getMeshCount() - 1 ? splats.size() : meshFirstSplatIdx[meshID + 1]) - meshFirstSplatIdx[meshID]; };

    { // Splat buffers
        std::vector<uint32_t> splatDescs;

        for (uint32_t instanceID = 0; instanceID < mpStaticScene->getInstanceCount(); ++instanceID)
        {
            const auto& instanceInfo = mpStaticScene->getInstanceInfos()[instanceID];
            uint32_t meshID = instanceInfo.meshID;
            uint32_t firstSplatIdx = meshFirstSplatIdx[meshID];
            uint32_t splatCount = getMeshSplatCount(meshID);

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

    { // Splat BLAS & TLAS
        auto pSplatIcosahedronVertexBuffer = getDevice()->createStructuredBuffer(
            sizeof(float3) * Icosahedron::kVertexCount,
            splatIcosahedronVertices.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splatIcosahedronVertices.data()
        );
        auto pSplatIcosahedronIndexBuffer = getDevice()->createStructuredBuffer(
            sizeof(uint3) * Icosahedron::kTriangleCount,
            splatIcosahedronIndices.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splatIcosahedronIndices.data()
        );

        mpSplatBLASs = BLASBuilder::build(
            getDevice()->getRenderContext(),
            [&]
            {
                auto splatIcosahedronVertexBufferAddr = pSplatIcosahedronVertexBuffer->getGpuAddress();
                auto splatIcosahedronIndexBufferAddr = pSplatIcosahedronIndexBuffer->getGpuAddress();

                std::vector<BLASBuildDesc> blasBuildDescs(mpStaticScene->getMeshCount());
                for (uint32_t meshID = 0; meshID < mpStaticScene->getMeshCount(); ++meshID)
                {
                    auto& blasBuildDesc = blasBuildDescs[meshID];

                    uint32_t meshSplatCount = getMeshSplatCount(meshID);

                    blasBuildDesc.geomDescs.push_back(
                        RtGeometryDesc{
                            .type = RtGeometryType::Triangles,
                            .flags = RtGeometryFlags::None, // Opaque?
                            .content =
                                {.triangles =
                                     {
                                         .transform3x4 = 0,
                                         .indexFormat = ResourceFormat::R32Uint,
                                         .vertexFormat = ResourceFormat::RGB32Float,
                                         .indexCount = meshSplatCount * Icosahedron::kTriangleCount * 3,
                                         .vertexCount = 0, // Seems to work (at least on Vulkan)
                                         .indexData = splatIcosahedronIndexBufferAddr,
                                         .vertexData = splatIcosahedronVertexBufferAddr,
                                         .vertexStride = sizeof(float3),
                                     }}
                        }
                    );
                    splatIcosahedronIndexBufferAddr += meshSplatCount * Icosahedron::kTriangleCount * sizeof(uint3);
                    splatIcosahedronVertexBufferAddr += meshSplatCount * Icosahedron::kVertexCount * sizeof(float3);
                }
                return blasBuildDescs;
            }()
        );

        mpSplatTLAS = TLASBuilder::build(
            getDevice()->getRenderContext(),
            [&]
            {
                TLASBuildDesc buildDesc;
                buildDesc.instanceDescs.resize(mpStaticScene->getInstanceCount());
                for (uint instanceID = 0; instanceID < mpStaticScene->getInstanceCount(); ++instanceID)
                {
                    const auto& instanceInfo = mpStaticScene->getInstanceInfos()[instanceID];
                    auto instanceDesc = RtInstanceDesc{
                        .instanceID = instanceInfo.meshID, // Custom InstanceID
                        .instanceMask = 0xFF,
                        .instanceContributionToHitGroupIndex = 0,
                        .flags = RtGeometryInstanceFlags::None,
                        .accelerationStructure = mpSplatBLASs[instanceInfo.meshID]->getGpuAddress(),
                    };
                    auto transform3x4 = instanceInfo.transform.getMatrix3x4();
                    std::memcpy(instanceDesc.transform, &transform3x4, sizeof(instanceDesc.transform));
                    static_assert(sizeof(instanceDesc.transform) == sizeof(transform3x4));
                    buildDesc.instanceDescs[instanceID] = instanceDesc;
                }
                return buildDesc;
            }()
        );
    }
}

GS3DIndLight::GS3DIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    mpMiscRenderer = make_ref<GS3DMiscRenderer>(getDevice());
}

void GS3DIndLight::update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
{
    mpStaticScene = pDefaultStaticScene;
    if (isSceneChanged)
        onSceneChanged();
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