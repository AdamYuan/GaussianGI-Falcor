//
// Created by adamyuan on 12/17/24.
//

#include "GS3DIndLight.hpp"

#include "GS3DIndLightSplat.hpp"
#include "GS3DIndLightAlgo.hpp"
#include "../../../Algorithm/GS3DBound.hpp"
#include "../../../Algorithm/MeshVHBVH.hpp"
#include "../../../Algorithm/Icosahedron.hpp"
#include "../../../Algorithm/Octahedron.hpp"
#include "../../../Util/BLASUtil.hpp"
#include "../../../Util/ShaderUtil.hpp"
#include "../../../Util/TextureUtil.hpp"
#include "../../../Util/TLASUtil.hpp"

namespace GSGI
{

namespace
{
constexpr uint32_t kDefaultSplatsPerMesh = 4096;
}

void GS3DIndLight::updateDrawResource(const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture)
{
    if (mConfig.useTracedShadow)
    {
        auto& pass = mDrawResource.traceShadowPass;
        if (!pass.pProgram)
        {
            ProgramDesc desc;
            desc.addShaderLibrary("GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightTraceShadow.rt.slang");
            desc.setMaxPayloadSize(sizeof(float));    // float T
            desc.setMaxAttributeSize(sizeof(float2)); // BuiltInTriangleIntersectionAttributes
            desc.setMaxTraceRecursionDepth(1);        // TraceRay is only called in rayGen()

            pass.pBindingTable = RtBindingTable::create(1, 1, 1);
            pass.pBindingTable->setRayGen(desc.addRayGen("rayGen"));
            pass.pBindingTable->setMiss(0, desc.addMiss("miss"));
            pass.pBindingTable->setHitGroup(0, 0, desc.addHitGroup("closestHit", "anyHit"));

            pass.pProgram = Program::create(getDevice(), desc);

            pass.pVars = RtProgramVars::create(getDevice(), pass.pProgram, pass.pBindingTable);
        }
    }
    else
    {
        if (!mDrawResource.pShadowPass)
            mDrawResource.pShadowPass =
                ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightShadow.cs.slang", "csMain");
    }

    if (!mDrawResource.pProbePass)
        mDrawResource.pProbePass =
            ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightProbe.cs.slang", "csMain");

    if (!mDrawResource.pCullPass)
        mDrawResource.pCullPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightCull.cs.slang", "csMain");

    if (!mDrawResource.pZNormalPass)
        mDrawResource.pZNormalPass =
            ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightZNormal.cs.slang", "csMain");

    if (!mDrawResource.pDrawPass)
    {
        ProgramDesc splatDrawDesc;
        splatDrawDesc.addShaderLibrary("GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightDraw.3d.slang")
            .vsEntry("vsMain")
            .gsEntry("gsMain")
            .psEntry("psMain");
        mDrawResource.pDrawPass = RasterPass::create(getDevice(), splatDrawDesc);
        mDrawResource.pDrawPass->getState()->setVao(Vao::create(Vao::Topology::PointList));

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
    }

    if (!mDrawResource.pBlendPass)
        mDrawResource.pBlendPass =
            ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/GS3D/GS3DIndLightBlend.cs.slang", "csMain");

    if (bool updateBuffer =
            !mDrawResource.pSplatIDBuffer || mDrawResource.pSplatIDBuffer->getElementCount() != mInstancedSplatBuffer.splatCount;
        updateBuffer)
    {
        mDrawResource.pSplatIDBuffer = getDevice()->createStructuredBuffer(sizeof(uint32_t), mInstancedSplatBuffer.splatCount);
        mDrawResource.pSplatShadowBuffer = getDevice()->createTypedBuffer(ResourceFormat::R8Unorm, mInstancedSplatBuffer.splatCount);
        static constexpr std::size_t kProbeSize = 9 * sizeof(float16_t3);
        mDrawResource.pSrcSplatProbeBuffer = getDevice()->createStructuredBuffer(kProbeSize, mInstancedSplatBuffer.splatCount);
        mDrawResource.pDstSplatProbeBuffer = getDevice()->createStructuredBuffer(kProbeSize, mInstancedSplatBuffer.splatCount);
        mDrawResource.probeTick = 0;
    }

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
        mDrawResource.pDepthGSPPSplatDrawArgBuffer = getDevice()->createStructuredBuffer(
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
        mDrawResource.pZNormalTexture,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width,
                height,
                ResourceFormat::RG32Uint,
                1,
                1,
                nullptr,
                ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
            );
        }
    );

    updateTextureSize(
        mDrawResource.pSplatFbo,
        resolution,
        [&](uint width, uint height) { return Fbo::create(getDevice(), {pIndirectTexture}, args.pVBuffer->getDepthStencilTexture()); }
    );
}

ref<DepthStencilState> GS3DIndLight::getDepthStencilState(bool stencilTest, bool depthTest)
{
    static ref<DepthStencilState> pCached[4]{};
    uint32_t cacheID = uint32_t(stencilTest) << 1u | uint32_t(depthTest);
    if (pCached[cacheID] == nullptr)
    {
        DepthStencilState::Desc splatDSDesc;
        splatDSDesc.setDepthEnabled(depthTest);
        splatDSDesc.setDepthWriteMask(false);
        splatDSDesc.setStencilEnabled(stencilTest);
        if (stencilTest)
        {
            splatDSDesc.setStencilRef(1);
            splatDSDesc.setStencilFunc(DepthStencilState::Face::FrontAndBack, ComparisonFunc::Equal);
            splatDSDesc.setStencilOp(
                DepthStencilState::Face::FrontAndBack,
                DepthStencilState::StencilOp::Keep,
                DepthStencilState::StencilOp::Keep,
                DepthStencilState::StencilOp::Keep
            );
            splatDSDesc.setStencilReadMask(0xFF);
            splatDSDesc.setStencilWriteMask(0);
        }
        pCached[cacheID] = DepthStencilState::create(splatDSDesc);
    }
    return pCached[cacheID];
}

void GS3DIndLight::preprocessMeshSplats(std::vector<GS3DIndLightSplat>& meshSplats)
{
    // TODO: Morton-order sorting
    for (auto& splat : meshSplats)
    {
        splat.scale = math::abs(splat.scale);
        if (splat.rotate.w < 0)
            splat.rotate = -splat.rotate;
    }

    meshSplats.erase(
        std::remove_if(
            meshSplats.begin(), meshSplats.end(), [](const GS3DIndLightSplat& splat) { return math::any(splat.scale <= HLF_MIN); }
        ),
        meshSplats.end()
    );
}

template<typename AccelStructPrim_T>
void GS3DIndLight::onSceneChanged(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene)
{
    std::vector<GS3DIndLightPackedSplatGeom> splatGeoms;
    std::vector<GS3DIndLightPackedSplatAttrib> splatAttribs;
    std::vector<std::array<float3, AccelStructPrim_T::kVertexCount>> splatASPrimVertices;
    std::vector<std::array<uint3, AccelStructPrim_T::kTriangleCount>> splatASPrimIndices;

    std::vector<uint32_t> firstMeshSplatIdxs;
    firstMeshSplatIdxs.reserve(pStaticScene->getMeshCount());

    for (const auto& pMesh : pStaticScene->getMeshes())
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

            preprocessMeshSplats(meshSplats);

            return meshSplats;
        }();

        firstMeshSplatIdxs.push_back(splatGeoms.size());

        // For splat buffer
        for (const auto& splat : meshSplats)
        {
            splatGeoms.push_back(GS3DIndLightPackedSplatGeom::fromSplat(splat));
            splatAttribs.push_back(GS3DIndLightPackedSplatAttrib::fromSplat(splat));
        }

        // For splat BLAS & TLAS
        for (std::size_t meshSplatID = 0; meshSplatID < meshSplats.size(); ++meshSplatID)
        {
            const auto& splat = meshSplats[meshSplatID];

            float3x3 splatRotMat = math::matrixFromQuat(splat.rotate);

            std::array<float3, AccelStructPrim_T::kVertexCount> vertices = AccelStructPrim_T::kVertices;
            for (auto& vertex : vertices)
            {
                vertex /= AccelStructPrim_T::kFaceDist;
                vertex *= float3(splat.scale) * GS3DBound::kSqrt2Log10;
                vertex = math::mul(splatRotMat, vertex);
                vertex += splat.mean;
            }
            std::array<uint3, AccelStructPrim_T::kTriangleCount> indices = AccelStructPrim_T::kTriangles;
            for (uint32_t indexOffset = meshSplatID * AccelStructPrim_T::kVertexCount; auto& index : indices)
            {
                index.x += indexOffset;
                index.y += indexOffset;
                index.z += indexOffset;
            }

            splatASPrimVertices.push_back(vertices);
            splatASPrimIndices.push_back(indices);
        }
    }

    const auto getMeshSplatCount = [&](uint32_t meshID)
    {
        return (meshID == pStaticScene->getMeshCount() - 1 ? splatGeoms.size() : firstMeshSplatIdxs[meshID + 1]) -
               firstMeshSplatIdxs[meshID];
    };

    { // Splat buffers
        std::vector<uint32_t> splatDescs;
        std::vector<GS3DIndLightInstanceDesc> instanceDescs;

        for (uint32_t instanceID = 0; instanceID < pStaticScene->getInstanceCount(); ++instanceID)
        {
            const auto& instanceInfo = pStaticScene->getInstanceInfos()[instanceID];
            uint32_t meshID = instanceInfo.meshID;
            uint32_t firstMeshSplatIdx = firstMeshSplatIdxs[meshID];
            uint32_t splatCount = getMeshSplatCount(meshID);

            instanceDescs.push_back({
                .firstSplatIdx = (uint32_t)splatDescs.size(),
            });

            for (uint32_t splatOfst = 0; splatOfst < splatCount; ++splatOfst)
                splatDescs.push_back((firstMeshSplatIdx + splatOfst) | (instanceID << 24u));

            static_assert(GStaticScene::kMaxInstanceCount <= 256);
        }

        mInstancedSplatBuffer = {
            .pSplatGeomBuffer = getDevice()->createStructuredBuffer(
                sizeof(GS3DIndLightPackedSplatGeom), //
                splatGeoms.size(),
                ResourceBindFlags::ShaderResource,
                MemoryType::DeviceLocal,
                splatGeoms.data()
            ),
            .pSplatAttribBuffer = getDevice()->createStructuredBuffer(
                sizeof(GS3DIndLightPackedSplatAttrib), //
                splatAttribs.size(),
                ResourceBindFlags::ShaderResource,
                MemoryType::DeviceLocal,
                splatAttribs.data()
            ),
            .pSplatDescBuffer = getDevice()->createStructuredBuffer(
                sizeof(uint32_t), //
                splatDescs.size(),
                ResourceBindFlags::ShaderResource,
                MemoryType::DeviceLocal,
                splatDescs.data()
            ),
            .pInstanceDescBuffer = getDevice()->createStructuredBuffer(
                sizeof(GS3DIndLightInstanceDesc), //
                instanceDescs.size(),
                ResourceBindFlags::ShaderResource,
                MemoryType::DeviceLocal,
                instanceDescs.data()
            ),
            .splatCount = (uint32_t)splatDescs.size(),
        };
    }

    { // Splat BLAS & TLAS
        auto pSplatASPrimVertexBuffer = getDevice()->createStructuredBuffer(
            sizeof(float3) * AccelStructPrim_T::kVertexCount,
            splatASPrimVertices.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splatASPrimVertices.data()
        );
        auto pSplatASPrimIndexBuffer = getDevice()->createStructuredBuffer(
            sizeof(uint3) * AccelStructPrim_T::kTriangleCount,
            splatASPrimIndices.size(),
            ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            splatASPrimIndices.data()
        );

        mpSplatBLASs = BLASBuilder::build(
            pRenderContext,
            [&]
            {
                auto splatASPrimVertexBufferAddr = pSplatASPrimVertexBuffer->getGpuAddress();
                auto splatASPrimIndexBufferAddr = pSplatASPrimIndexBuffer->getGpuAddress();

                std::vector<BLASBuildDesc> blasBuildDescs(pStaticScene->getMeshCount());
                for (uint32_t meshID = 0; meshID < pStaticScene->getMeshCount(); ++meshID)
                {
                    auto& blasBuildDesc = blasBuildDescs[meshID];

                    uint32_t meshSplatCount = getMeshSplatCount(meshID);

                    blasBuildDesc.geomDescs.push_back(
                        RtGeometryDesc{
                            .type = RtGeometryType::Triangles,
                            .flags = RtGeometryFlags::Opaque,
                            .content =
                                {.triangles =
                                     {
                                         .transform3x4 = 0,
                                         .indexFormat = ResourceFormat::R32Uint,
                                         .vertexFormat = ResourceFormat::RGB32Float,
                                         .indexCount = meshSplatCount * AccelStructPrim_T::kTriangleCount * 3,
                                         .vertexCount = 0, // Seems to work (at least on Vulkan)
                                         .indexData = splatASPrimIndexBufferAddr,
                                         .vertexData = splatASPrimVertexBufferAddr,
                                         .vertexStride = sizeof(float3),
                                     }}
                        }
                    );
                    splatASPrimIndexBufferAddr += meshSplatCount * AccelStructPrim_T::kTriangleCount * sizeof(uint3);
                    splatASPrimVertexBufferAddr += meshSplatCount * AccelStructPrim_T::kVertexCount * sizeof(float3);
                }
                return blasBuildDescs;
            }()
        );

        mpSplatTLAS = TLASBuilder::build(
            pRenderContext,
            [&]
            {
                TLASBuildDesc buildDesc;
                buildDesc.instanceDescs.resize(pStaticScene->getInstanceCount());
                for (uint instanceID = 0; instanceID < pStaticScene->getInstanceCount(); ++instanceID)
                {
                    const auto& instanceInfo = pStaticScene->getInstanceInfos()[instanceID];
                    auto instanceDesc = RtInstanceDesc{
                        .instanceID = firstMeshSplatIdxs[instanceInfo.meshID], // Custom InstanceID
                        .instanceMask = 0xFF,
                        .instanceContributionToHitGroupIndex = 0,
                        .flags = RtGeometryInstanceFlags::TriangleFrontCounterClockwise,
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

GS3DIndLight::GS3DIndLight(const ref<GScene>& pScene) : GSceneObject(pScene)
{
    mpMiscRenderer = make_ref<GS3DMiscRenderer>(getDevice());
}

void GS3DIndLight::updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene)
{
    if (isSceneChanged)
    {
        enumVisit(
            mConfig.accelStructPrimitiveType,
            [&]<typename EnumInfo_T>(EnumInfo_T) { onSceneChanged<typename EnumInfo_T::Type>(pRenderContext, pStaticScene); }
        );
        mRunShadowPass = true;
    }
}

void GS3DIndLight::draw(
    RenderContext* pRenderContext,
    const ref<GStaticScene>& pStaticScene,
    const GIndLightDrawArgs& args,
    const ref<Texture>& pIndirectTexture
)
{
    if (mConfig.accelStructPrimitiveType != mPrevConfig.accelStructPrimitiveType)
    {
        // Re-build Accel Struct
        enumVisit(
            mConfig.accelStructPrimitiveType,
            [&]<typename EnumInfo_T>(EnumInfo_T) { onSceneChanged<typename EnumInfo_T::Type>(pRenderContext, pStaticScene); }
        );
    }

    updateDrawResource(args, pIndirectTexture);
    std::swap(mDrawResource.pSrcSplatProbeBuffer, mDrawResource.pDstSplatProbeBuffer);

    uint2 resolution = getTextureResolution2(pIndirectTexture);
    auto resolutionFloat = float2(resolution);

    // Splat Shadow Pass
    if (mConfig.useTracedShadow)
    {
        bool runPass = !mPrevConfig.useTracedShadow;
        runPass = runPass || math::any(mPrevTracedShadowDirection != pStaticScene->getLighting()->getData().direction);
        runPass = runPass || mRunShadowPass;
        mPrevTracedShadowDirection = pStaticScene->getLighting()->getData().direction;

        mRunShadowPass = false;

        if (runPass)
        {
            FALCOR_PROFILE(pRenderContext, "traceShadow");
            const auto& pass = mDrawResource.traceShadowPass;
            auto var = pass.pVars->getRootVar();
            pStaticScene->bindRootShaderData(var);
            mInstancedSplatBuffer.bindShaderData(var["gSplats"]);
            var["gSplatAccel"].setAccelerationStructure(mpSplatTLAS);
            var["gSplatShadows"] = mDrawResource.pSplatShadowBuffer;
            setGS3DAccelStructPrimitiveDefine(pass.pProgram, mConfig.accelStructPrimitiveType);

            pRenderContext->raytrace(pass.pProgram.get(), pass.pVars.get(), mInstancedSplatBuffer.splatCount, 1, 1);
        }
    }
    else
    {
        FALCOR_PROFILE(pRenderContext, "shadow");
        auto [prog, var] = getShaderProgVar(mDrawResource.pShadowPass);
        pStaticScene->bindRootShaderData(var);
        mInstancedSplatBuffer.bindShaderData(var["gSplats"]);
        args.pShadow->prepareProgram(prog, var["gGShadow"], args.shadowType);
        var["gSplatShadows"] = mDrawResource.pSplatShadowBuffer;

        mDrawResource.pShadowPass->execute(pRenderContext, mInstancedSplatBuffer.splatCount, 1, 1);
    }

    // Splat Probe Pass
    {
        FALCOR_PROFILE(pRenderContext, "probe");
        auto [prog, var] = getShaderProgVar(mDrawResource.pProbePass);
        static constexpr uint32_t kRaysPerProbe = 32;
        pStaticScene->bindRootShaderData(var);
        mInstancedSplatBuffer.bindShaderData(var["gSplats"]);
        var["gSplatShadows"] = mDrawResource.pSplatShadowBuffer;
        var["gPrevSplatProbes"] = mDrawResource.pSrcSplatProbeBuffer;
        var["gSplatProbes"] = mDrawResource.pDstSplatProbeBuffer;
        var["gSplatAccel"].setAccelerationStructure(mpSplatTLAS);
        var["gTick"] = mDrawResource.probeTick;
        prog->addDefine("VNDF_SAMPLE", mConfig.vndfSample ? "1" : "0");
        setGS3DAccelStructPrimitiveDefine(prog, mConfig.accelStructPrimitiveType);

        mDrawResource.pProbePass->execute(pRenderContext, mInstancedSplatBuffer.splatCount * kRaysPerProbe, 1, 1);
    }

    // Reset
    static_assert(offsetof(DrawArguments, InstanceCount) == sizeof(uint32_t));
    mDrawResource.pSplatDrawArgBuffer->setElement<uint32_t>(offsetof(DrawArguments, InstanceCount) / sizeof(uint32_t), 0u);
    if (mConfig.useDepthGSPP)
        mDrawResource.pDepthGSPPSplatDrawArgBuffer->setElement<uint32_t>(offsetof(DrawArguments, InstanceCount) / sizeof(uint32_t), 0u);

    // Splat Cull Pass
    {
        FALCOR_PROFILE(pRenderContext, "cull");
        auto [prog, var] = getShaderProgVar(mDrawResource.pCullPass);
        pStaticScene->bindRootShaderData(var);
        mInstancedSplatBuffer.bindShaderData(var["gSplats"]);
        var["gResolution"] = resolutionFloat;

        var["gSplatDrawArgs"] = mDrawResource.pSplatDrawArgBuffer;
        var["gDepthGSPPSplatDrawArgs"] = mDrawResource.pDepthGSPPSplatDrawArgBuffer;
        var["gSplatIDs"] = mDrawResource.pSplatIDBuffer;
        setGS3DPrimitiveTypeDefine(prog, mConfig.primitiveType);
        prog->addDefine("USE_DEPTH_GSPP", mConfig.useDepthGSPP ? "1" : "0");

        mDrawResource.pCullPass->execute(pRenderContext, mInstancedSplatBuffer.splatCount, 1, 1);
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

    // Z-Normal Pass
    if (mConfig.useZNormal)
    {
        FALCOR_PROFILE(pRenderContext, "zNormal");
        auto [prog, var] = getShaderProgVar(mDrawResource.pZNormalPass);
        pStaticScene->bindRootShaderData(var);
        var["gZNormals"] = mDrawResource.pZNormalTexture;
        args.pVBuffer->bindShaderData(var["gGVBuffer"]);

        mDrawResource.pZNormalPass->execute(pRenderContext, resolution.x, resolution.y, 1);
    }

    // Splat Draw Pass
    {
        FALCOR_PROFILE(pRenderContext, "draw");
        pRenderContext->clearRtv(pIndirectTexture->getRTV().get(), float4{});
        auto [prog, var] = getShaderProgVar(mDrawResource.pDrawPass);
        pStaticScene->bindRootShaderData(var);
        mInstancedSplatBuffer.bindShaderData(var["gSplats"]);
        var["gSplatIDs"] = mDrawResource.pSplatIDBuffer;
        var["gResolution"] = resolutionFloat;
        var["gSplatShadows"] = mDrawResource.pSplatShadowBuffer;
        var["gSplatProbes"] = mDrawResource.pDstSplatProbeBuffer;
        var["gZNormals"] = mDrawResource.pZNormalTexture;
        args.pVBuffer->bindShaderData(var["gGVBuffer"]);
        if (mConfig.useDepthGSPP)
            setGS3DPrimitiveTypeDefine(prog, GS3DPrimitiveType::kGSPP);
        else
            setGS3DPrimitiveTypeDefine(prog, mConfig.primitiveType);
        prog->addDefine("USE_Z_NORMAL", mConfig.useZNormal ? "1" : "0");

        mDrawResource.pDrawPass->getState()->setFbo(mDrawResource.pSplatFbo);
        mDrawResource.pDrawPass->getState()->setDepthStencilState(getDepthStencilState(mConfig.useStencil, false));
        prog->addDefine("USE_DEPTH_GSPP", "0");
        pRenderContext->drawIndirect(
            mDrawResource.pDrawPass->getState().get(),
            mDrawResource.pDrawPass->getVars().get(),
            1,
            mDrawResource.pSplatDrawArgBuffer.get(),
            0,
            nullptr,
            0
        );

        if (mConfig.useDepthGSPP)
        {
            FALCOR_PROFILE(pRenderContext, "draw depth GS++");
            mDrawResource.pDrawPass->getState()->setDepthStencilState(getDepthStencilState(mConfig.useStencil, true));
            prog->addDefine("USE_DEPTH_GSPP", "1");
            pRenderContext->drawIndirect(
                mDrawResource.pDrawPass->getState().get(),
                mDrawResource.pDrawPass->getVars().get(),
                1,
                mDrawResource.pDepthGSPPSplatDrawArgBuffer.get(),
                0,
                nullptr,
                0
            );
        }
    }

    // OIT Blend Pass
    {
        FALCOR_PROFILE(pRenderContext, "blend");
        auto [prog, var] = getShaderProgVar(mDrawResource.pBlendPass);
        var["gResolution"] = resolution;
        var["gIndirect"] = pIndirectTexture;

        mDrawResource.pBlendPass->execute(pRenderContext, resolution.x, resolution.y, 1);
    }

    ++mDrawResource.probeTick;
    mPrevConfig = mConfig;
}

void GS3DIndLight::drawMisc(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, const ref<Fbo>& pTargetFbo)
{
    mpMiscRenderer->draw(
        pRenderContext,
        pTargetFbo,
        {
            .pStaticScene = pStaticScene,
            .instancedSplatBuffer = mInstancedSplatBuffer,
            .pSplatTLAS = mpSplatTLAS,
            .pSplatShadowBuffer = mDrawResource.pSplatShadowBuffer,
            .pSplatProbeBuffer = mDrawResource.pDstSplatProbeBuffer,
            .accelStructPrimitiveType = mConfig.accelStructPrimitiveType,
        }
    );
}

void GS3DIndLight::renderUIImpl(Gui::Widgets& widget)
{
    if (widget.button("Reset Probe Tick"))
        mDrawResource.probeTick = 0u;

    enumDropdown(widget, "AccelStruct Primitive Type", mConfig.accelStructPrimitiveType);

    widget.checkbox("Use Traced Shadow", mConfig.useTracedShadow);
    widget.checkbox("Use Stencil", mConfig.useStencil);
    widget.checkbox("Use DepthGS++", mConfig.useDepthGSPP);
    widget.checkbox("Use Z-Normal", mConfig.useZNormal);
    widget.checkbox("VNDF Sampling", mConfig.vndfSample);
    if (!mConfig.useDepthGSPP)
        enumDropdown(widget, "Primitive Type", mConfig.primitiveType);

    if (auto g = widget.group("Misc", true))
        mpMiscRenderer->renderUI(g);

    widget.text(fmt::format("Splat Count: {}", mInstancedSplatBuffer.splatCount));

    /* auto result = MeshClosestPoint::query(
        GMeshView{pStaticScene->getMeshes()[0]}, meshBvh, pStaticScene->getScene()->getCamera()->getPosition(), 1.0f
    );
    widget.text(fmt::format("dist = {}, id = {}", math::sqrt(result.dist2), result.optPrimitiveID)); */
}

} // namespace GSGI