//
// Created by adamyuan on 12/23/24.
//

#include "GS3DMiscRenderer.hpp"

#include "GS3DIndLightSplat.hpp"
#include "../../../Util/ShaderUtil.hpp"
#include "../../../Util/TextureUtil.hpp"

namespace GSGI
{

GS3DMiscRenderer::GS3DMiscRenderer(const ref<Device>& pDevice) : GDeviceObject(pDevice) {}

void GS3DMiscRenderer::initialize()
{
    mSplatViewSorter = DeviceSorter<DeviceSortDispatchType::kIndirect>{
        getDevice(),
        DeviceSortDesc({
            DeviceSortBufferType::kKey32,
            DeviceSortBufferType::kPayload,
        })
    };

    auto pPointVao = Vao::create(Vao::Topology::PointList);

    mpPointPass = RasterPass::create(getDevice(), "GaussianGI/Renderer/IndLight/3DGS/GS3DMiscPoint.3d.slang", "vsMain", "psMain");
    mpPointPass->getState()->setVao(pPointVao);

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

    mpSplatViewPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/3DGS/GS3DMiscSplatView.cs.slang", "csMain");
    ProgramDesc splatDrawDesc;
    splatDrawDesc.addShaderLibrary("GaussianGI/Renderer/IndLight/3DGS/GS3DMiscSplatDraw.3d.slang")
        .vsEntry("vsMain")
        .gsEntry("gsMain")
        .psEntry("psMain");
    mpSplatDrawPass = RasterPass::create(getDevice(), splatDrawDesc);
    mpSplatDrawPass->getState()->setVao(pPointVao);
    mpSplatDrawPass->getState()->setRasterizerState(RasterizerState::create(splatRasterDesc));
    mpSplatDrawPass->getState()->setBlendState(BlendState::create(splatBlendDesc));
    mpSplatDrawPass->getState()->setDepthStencilState(DepthStencilState::create(splatDepthDesc));

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

void GS3DMiscRenderer::draw(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args)
{
    if (!mIsInitialized)
    {
        initialize();
        mIsInitialized = true;
    }
    FALCOR_PROFILE(pRenderContext, "GS3DMisc");

    pRenderContext->clearTexture(pTargetFbo->getColorTexture(0).get(), float4{});

    if (mConfig.type == GS3DMiscType::kPoint) // Point
    {
        pRenderContext->clearDsv(pTargetFbo->getDepthStencilView().get(), 1.0f, 0, true, false);

        FALCOR_PROFILE(pRenderContext, "drawPoint");
        auto [prog, var] = getShaderProgVar(mpPointPass);
        args.pStaticScene->bindRootShaderData(var);
        var["gSplats"] = args.pSplatBuffer;
        var["gSplatsPerMesh"] = args.splatsPerMesh;

        mpPointPass->getState()->setFbo(pTargetFbo);
        pRenderContext->draw(
            mpPointPass->getState().get(), mpPointPass->getVars().get(), args.splatsPerMesh * args.pStaticScene->getInstanceCount(), 0
        );
    }
    else // Splat
    {
        uint splatViewCount = args.splatsPerMesh * args.pStaticScene->getInstanceCount();
        FALCOR_CHECK(splatViewCount != 0, "");
        if (splatViewCount != mSplatViewCount)
        {
            mSplatViewCount = splatViewCount;

            mpSplatViewBuffer = getDevice()->createStructuredBuffer(sizeof(GS3DIndLightSplatView), mSplatViewCount);
            mpSplatViewSortKeyBuffer = getDevice()->createStructuredBuffer(sizeof(uint32_t), mSplatViewCount);
            mpSplatViewSortPayloadBuffer = getDevice()->createStructuredBuffer(sizeof(uint32_t), mSplatViewCount);
            mSplatViewSortResource =
                DeviceSortResource<DeviceSortDispatchType::kIndirect>::create(getDevice(), mSplatViewSorter.getDesc(), splatViewCount);
        }

        // Reset Draw Args
        static_assert(offsetof(gfx::IndirectDrawArguments, InstanceCount) == sizeof(uint32_t));
        mpSplatViewDrawArgBuffer->setElement<uint32_t>(offsetof(gfx::IndirectDrawArguments, InstanceCount) / sizeof(uint32_t), 0u);

        float2 resolutionFloat = float2(getTextureResolution2(pTargetFbo));

        // Splat View Pass
        {
            FALCOR_PROFILE(pRenderContext, "splatView");
            auto [prog, var] = getShaderProgVar(mpSplatViewPass);
            args.pStaticScene->bindRootShaderData(var);
            var["gSplats"] = args.pSplatBuffer;
            var["gSplatsPerMesh"] = args.splatsPerMesh;
            var["gResolution"] = resolutionFloat;

            var["gSplatViews"] = mpSplatViewBuffer;
            var["gSplatViewDrawArgs"] = mpSplatViewDrawArgBuffer;
            var["gSplatViewSortKeys"] = mpSplatViewSortKeyBuffer;
            var["gSplatViewSortPayloads"] = mpSplatViewSortPayloadBuffer;

            mpSplatViewPass->execute(pRenderContext, mSplatViewCount, 1, 1);
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