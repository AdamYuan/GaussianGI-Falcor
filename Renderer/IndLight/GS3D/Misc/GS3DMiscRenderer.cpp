//
// Created by adamyuan on 12/23/24.
//

#include "GS3DMiscRenderer.hpp"

#include "../../../../Util/ShaderUtil.hpp"
#include "../../../../Util/TextureUtil.hpp"

namespace GSGI
{

GS3DMiscRenderer::GS3DMiscRenderer(const ref<Device>& pDevice) : GDeviceObject(pDevice) {}

void GS3DMiscRenderer::drawPoints(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args)
{
    if (!mpPointPass)
    {
        mpPointPass = RasterPass::create(getDevice(), "GaussianGI/Renderer/IndLight/GS3D/Misc/GS3DMiscPoint.3d.slang", "vsMain", "psMain");
        mpPointPass->getState()->setVao(Vao::create(Vao::Topology::PointList));
    }
    pRenderContext->clearRtv(pTargetFbo->getColorTexture(0)->getRTV().get(), float4{});
    pRenderContext->clearDsv(pTargetFbo->getDepthStencilView().get(), 1.0f, 0, true, false);

    uint splatCount = args.instancedSplatBuffer.splatCount;

    FALCOR_PROFILE(pRenderContext, "drawPoint");
    auto [prog, var] = getShaderProgVar(mpPointPass);
    args.pStaticScene->bindRootShaderData(var);
    args.instancedSplatBuffer.bindShaderData(var["gSplats"]);

    mpPointPass->getState()->setFbo(pTargetFbo);
    pRenderContext->draw(mpPointPass->getState().get(), mpPointPass->getVars().get(), splatCount, 0);
}

void GS3DMiscRenderer::drawSplats(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args)
{
    if (!mSplatViewSorter.isInitialized())
    {
        mSplatViewSorter = DeviceSorter<DeviceSortDispatchType::kIndirect>{
            getDevice(),
            DeviceSortDesc({
                DeviceSortBufferType::kKey32,
                DeviceSortBufferType::kPayload,
            })
        };
    }
    if (!mpSplatViewPass)
        mpSplatViewPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/GS3D/Misc/GS3DMiscSplatView.cs.slang", "csMain");
    if (!mpSplatDrawPass)
    {
        DepthStencilState::Desc splatDepthDesc;
        splatDepthDesc.setDepthEnabled(false);
        BlendState::Desc splatBlendDesc;
        splatBlendDesc.setRtBlend(0, true).setRtParams(
            0,
            BlendState::BlendOp::Add,
            BlendState::BlendOp::Add,
            BlendState::BlendFunc::SrcAlpha,
            BlendState::BlendFunc::OneMinusSrcAlpha,
            BlendState::BlendFunc::One,
            BlendState::BlendFunc::OneMinusSrcAlpha
        );
        RasterizerState::Desc splatRasterDesc;
        splatRasterDesc.setCullMode(RasterizerState::CullMode::None);

        ProgramDesc splatDrawDesc;
        splatDrawDesc.addShaderLibrary("GaussianGI/Renderer/IndLight/GS3D/Misc/GS3DMiscSplatDraw.3d.slang")
            .vsEntry("vsMain")
            .gsEntry("gsMain")
            .psEntry("psMain");
        mpSplatDrawPass = RasterPass::create(getDevice(), splatDrawDesc);
        mpSplatDrawPass->getState()->setVao(Vao::create(Vao::Topology::PointList));
        mpSplatDrawPass->getState()->setRasterizerState(RasterizerState::create(splatRasterDesc));
        mpSplatDrawPass->getState()->setBlendState(BlendState::create(splatBlendDesc));
        mpSplatDrawPass->getState()->setDepthStencilState(DepthStencilState::create(splatDepthDesc));
    }
    if (!mpSplatViewDrawArgBuffer)
    {
        gfx::IndirectDrawArguments splatViewDrawArgs = {
            .VertexCountPerInstance = 1,
            .InstanceCount = 0,
            .StartVertexLocation = 0,
            .StartInstanceLocation = 0,
        };
        mpSplatViewDrawArgBuffer = getDevice()->createStructuredBuffer(
            sizeof(gfx::IndirectDrawArguments),
            1,
            ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess | ResourceBindFlags::IndirectArg,
            MemoryType::DeviceLocal,
            &splatViewDrawArgs
        );
        static_assert(sizeof(gfx::IndirectDrawArguments) == 4 * sizeof(uint32_t));
    }
    uint splatViewCount = args.instancedSplatBuffer.splatCount;
    FALCOR_CHECK(splatViewCount != 0, "");
    if (!mpSplatViewBuffer || splatViewCount != mpSplatViewBuffer->getElementCount())
    {
        mpSplatViewBuffer = getDevice()->createStructuredBuffer(sizeof(GS3DIndLightMiscSplatView), splatViewCount);
        mpSplatViewSortKeyBuffer = getDevice()->createStructuredBuffer(sizeof(uint32_t), splatViewCount);
        mpSplatViewSortPayloadBuffer = getDevice()->createStructuredBuffer(sizeof(uint32_t), splatViewCount);
        mSplatViewSortResource =
            DeviceSortResource<DeviceSortDispatchType::kIndirect>::create(getDevice(), mSplatViewSorter.getDesc(), splatViewCount);
    }

    // Reset Draw Args
    static_assert(offsetof(gfx::IndirectDrawArguments, InstanceCount) == sizeof(uint32_t));
    mpSplatViewDrawArgBuffer->setElement<uint32_t>(offsetof(gfx::IndirectDrawArguments, InstanceCount) / sizeof(uint32_t), 0u);

    float2 resolutionFloat = float2(getTextureResolution2(pTargetFbo));

    pRenderContext->clearRtv(pTargetFbo->getColorTexture(0)->getRTV().get(), float4{});

    // Splat View Pass
    {
        FALCOR_PROFILE(pRenderContext, "splatView");
        auto [prog, var] = getShaderProgVar(mpSplatViewPass);
        args.pStaticScene->bindRootShaderData(var);
        args.instancedSplatBuffer.bindShaderData(var["gSplats"]);
        var["gResolution"] = resolutionFloat;

        var["gSplatViews"] = mpSplatViewBuffer;
        var["gSplatViewDrawArgs"] = mpSplatViewDrawArgBuffer;
        var["gSplatViewSortKeys"] = mpSplatViewSortKeyBuffer;
        var["gSplatViewSortPayloads"] = mpSplatViewSortPayloadBuffer;

        mpSplatViewPass->execute(pRenderContext, splatViewCount, 1, 1);
    }

    // Sort
    {
        FALCOR_PROFILE(pRenderContext, "sortSplat");
        mSplatViewSorter.dispatch(
            pRenderContext,
            {mpSplatViewSortKeyBuffer, mpSplatViewSortPayloadBuffer},
            mpSplatViewDrawArgBuffer,
            offsetof(gfx::IndirectDrawArguments, InstanceCount),
            mSplatViewSortResource
        );
    }

    // Splat Draw Pass
    {
        FALCOR_PROFILE(pRenderContext, "drawSplat");
        auto [prog, var] = getShaderProgVar(mpSplatDrawPass);
        args.pStaticScene->bindRootShaderData(var);
        args.instancedSplatBuffer.bindShaderData(var["gSplats"]);
        var["gSplatViews"] = mpSplatViewBuffer;
        var["gSplatViewSortPayloads"] = mpSplatViewSortPayloadBuffer;
        var["gResolution"] = resolutionFloat;
        float alpha = mConfig.splatOpacity;
        float d = math::sqrt(math::log(math::max(255.0f * alpha, 1.0f)));
        var["gAlpha"] = alpha;
        var["gD"] = d;

        mpSplatDrawPass->getState()->setFbo(pTargetFbo);
        pRenderContext->drawIndirect(
            mpSplatDrawPass->getState().get(), mpSplatDrawPass->getVars().get(), 1, mpSplatViewDrawArgBuffer.get(), 0, nullptr, 0
        );
    }
}

void GS3DMiscRenderer::drawTracedSplats(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args)
{
    uint2 resolution = getTextureResolution2(pTargetFbo);

    updateTextureSize(
        mpSplatTraceColorTexture,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width,
                height,
                ResourceFormat::RGBA8Unorm,
                1,
                1,
                nullptr,
                ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
            );
        }
    );

    if (!mSplatTracePass.pProgram)
    {
        ProgramDesc desc;
        desc.addShaderLibrary("GaussianGI/Renderer/IndLight/GS3D/Misc/GS3DMiscTraceSplat.rt.slang");
        desc.setMaxPayloadSize(sizeof(uint32_t)); // TinyUniformSampleGenerator
        desc.setMaxAttributeSize(sizeof(float2)); // BuiltInTriangleIntersectionAttributes
        desc.setMaxTraceRecursionDepth(1);        // TraceRay is only called in rayGen()

        mSplatTracePass.pBindingTable = RtBindingTable::create(1, 1, 1);
        mSplatTracePass.pBindingTable->setRayGen(desc.addRayGen("rayGen"));
        mSplatTracePass.pBindingTable->setMiss(0, desc.addMiss("miss"));
        mSplatTracePass.pBindingTable->setHitGroup(0, 0, desc.addHitGroup("closestHit", "anyHit"));

        mSplatTracePass.pProgram = Program::create(getDevice(), desc);

        mSplatTracePass.pVars = RtProgramVars::create(getDevice(), mSplatTracePass.pProgram, mSplatTracePass.pBindingTable);
    }

    {
        FALCOR_PROFILE(pRenderContext, "traceSplat");

        auto var = mSplatTracePass.pVars->getRootVar();
        args.pStaticScene->bindRootShaderData(var);
        args.instancedSplatBuffer.bindShaderData(var["gSplats"]);
        var["gSplatAccel"].setAccelerationStructure(args.pSplatTLAS);
        var["gColor"] = mpSplatTraceColorTexture;

        pRenderContext->raytrace(mSplatTracePass.pProgram.get(), mSplatTracePass.pVars.get(), resolution.x, resolution.y, 1u);
    }

    pRenderContext->blit(mpSplatTraceColorTexture->getSRV(), pTargetFbo->getColorTexture(0)->getRTV());
}

void GS3DMiscRenderer::draw(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args)
{
    if (mConfig.type == GS3DMiscType::kPoint)
        drawPoints(pRenderContext, pTargetFbo, args);
    else if (mConfig.type == GS3DMiscType::kSplat)
        drawSplats(pRenderContext, pTargetFbo, args);
    else if (mConfig.type == GS3DMiscType::kTracedSplat)
        drawTracedSplats(pRenderContext, pTargetFbo, args);
}

void GS3DMiscRenderer::renderUIImpl(Gui::Widgets& widget)
{
    enumDropdown(widget, "Type", mConfig.type);
    if (mConfig.type == GS3DMiscType::kSplat)
    {
        widget.var("Splat Opacity", mConfig.splatOpacity, 0.0f, 1.0f);
    }
}

} // namespace GSGI