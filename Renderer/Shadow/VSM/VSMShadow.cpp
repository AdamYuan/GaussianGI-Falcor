//
// Created by adamyuan on 12/12/24.
//

#include "VSMShadow.hpp"

#include "../../../Algorithm/GaussianBlurKernel.hpp"
#include "../../../Util/ShaderUtil.hpp"
#include "../../../Util/TextureUtil.hpp"

namespace GSGI
{

VSMShadow::VSMShadow(const ref<GScene>& pScene) : GSceneObject(pScene) {}

void VSMShadow::updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, bool isLightChanged)
{
    bool doUpdate = isSceneChanged || isLightChanged || mConfig != mPrevConfig;
    mPrevConfig = mConfig;

    if (!doUpdate)
        return;

    mTransform = ShadowMapTransform::create(pStaticScene->getBound(), pStaticScene->getLighting()->getData().direction);

    if (!mpDrawPass)
    {
        ProgramDesc rasterDesc;
        rasterDesc.addShaderLibrary("GaussianGI/Renderer/Shadow/VSM/VSMDraw.3d.slang")
            .vsEntry("vsMain")
            .gsEntry("gsMain")
            .psEntry("psMain");
        mpDrawPass = RasterPass::create(getDevice(), rasterDesc);
        mpDrawPass->getState()->setRasterizerState(GMesh::getRasterizerState());
    }

    if (!mpBlurPasses[0])
    {
        const auto createBlurPass = [this](uint blurAxis)
        {
            DefineList defList;
            defList.add("MAX_BLUR_RADIUS", fmt::to_string(Config::kMaxBlurRadius));
            defList.add("BLUR_AXIS", fmt::to_string(blurAxis));
            return ComputePass::create(getDevice(), "GaussianGI/Renderer/Shadow/VSM/VSMBlur.cs.slang", "csMain", defList);
        };
        mpBlurPasses[0] = createBlurPass(0);
        mpBlurPasses[1] = createBlurPass(1);
    }

    auto resolution = uint2{mConfig.resolution};

    updateTextureSize(
        mpDepthBuffer,
        resolution,
        [this](uint width, uint height)
        {
            return getDevice()->createTexture2D(
                width, height, ResourceFormat::D32Float, 1, 1, nullptr, ResourceBindFlags::DepthStencil | ResourceBindFlags::ShaderResource
            );
        }
    );

    const auto createTexture = [this](uint width, uint height)
    {
        return getDevice()->createTexture2D(
            width,
            height,
            ResourceFormat::RG16Unorm,
            1,
            1,
            nullptr,
            ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
        );
    };
    updateTextureSize(mpTextures[0], resolution, createTexture);
    updateTextureSize(mpTextures[1], resolution, createTexture);

    updateTextureSize(
        mpFbo, resolution, [this](uint width, uint height) { return Fbo::create(getDevice(), {mpTextures[0]}, mpDepthBuffer); }
    );

    { // Draw
        auto [prog, var] = getShaderProgVar(mpDrawPass);
        mTransform.bindShaderData(var["gSMTransform"]);

        pRenderContext->clearDsv(mpFbo->getDepthStencilView().get(), 1.0f, 0, true, false);
        pRenderContext->clearRtv(mpFbo->getRenderTargetView(0).get(), float4{1.0f});
        pStaticScene->draw(pRenderContext, mpFbo, mpDrawPass);
    }

    // Blur
    if (mConfig.blurRadius > 0)
    {
        GaussianBlurKernel blurKernel = GaussianBlurKernel::create(mConfig.blurRadius);
        for (uint blurAxis = 0; blurAxis <= 1; ++blurAxis)
        {
            auto [prog, var] = getShaderProgVar(mpBlurPasses[blurAxis]);
            blurKernel.bindShaderWeights(var["gWeights"]);
            var["gRadius"] = blurKernel.radius;
            var["gResolution"] = resolution;
            var["gSrc"] = mpTextures[blurAxis];
            var["gDst"] = mpTextures[blurAxis ^ 1];

            mpBlurPasses[blurAxis]->execute(pRenderContext, resolution.x, resolution.y, 1);
        }
    }
}

void VSMShadow::bindShaderData(const ShaderVar& var) const
{
    var["shadowMap"] = mpTextures[0];
    var["smSampler"] = getDevice()->getDefaultSampler();
    var["vsmBias"] = mSampleConfig.vsmBias;
    var["bleedReduction"] = mSampleConfig.bleedReduction;
    mTransform.bindShaderData(var["smTransform"]);
}

void VSMShadow::renderUIImpl(Gui::Widgets& widget)
{
    widget.var("Dim", mConfig.resolution, Config::kMinResolution, Config::kMaxResolution);
    widget.var("Blur Radius", mConfig.blurRadius, 0u, Config::kMaxBlurRadius);
    widget.var("VSM-Bias", mSampleConfig.vsmBias, 0.0f);
    widget.var("Bleed Reduc.", mSampleConfig.bleedReduction, 0.0f, 1.0f);
}

} // namespace GSGI