//
// Created by adamyuan on 12/15/24.
//

#include "PTIndLight.hpp"

#include "../../../Util/ShaderUtil.hpp"

namespace GSGI
{

PTIndLight::PTIndLight(const ref<GScene>& pScene) : GSceneObject(pScene) {}

void PTIndLight::updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene)
{
    if (isSceneChanged)
        mSPP = 0;

    // if (!isActive)
    //     mSPP = 0;
}

void PTIndLight::draw(
    RenderContext* pRenderContext,
    const ref<GStaticScene>& pStaticScene,
    const GIndLightDrawArgs& args,
    const ref<Texture>& pIndirectTexture
)
{
    if (bool isCameraChanged = pStaticScene->getCamera()->getData().viewProjMat != mViewProjMat,
        isLightingChanged = pStaticScene->getLighting()->getData() != mLightingData,
        isResolutionChanged = math::any(args.pVBuffer->getResolution() != mResolution);
        isCameraChanged || isLightingChanged || isResolutionChanged)
    {
        mViewProjMat = pStaticScene->getCamera()->getData().viewProjMat;
        mLightingData = pStaticScene->getLighting()->getData();
        mResolution = args.pVBuffer->getResolution();
        mSPP = 0;
    }

    if (!mpPass)
        mpPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/PathTraced/PTIndLight.cs.slang", "csMain");
    auto [prog, var] = getShaderProgVar(mpPass);
    var["gIndLight"] = pIndirectTexture;
    var["gSPP"] = mSPP;
    var["gMaxBounce"] = mConfig.maxBounce;
    pStaticScene->bindRootShaderData(var);
    args.pVBuffer->bindShaderData(var["gGVBuffer"]);
    args.pShadow->prepareProgram(prog, var["gGShadow"], args.shadowType);
    mpPass->execute(pRenderContext, mResolution.x, mResolution.y, 1);

    ++mSPP;
}

void PTIndLight::renderUIImpl(Gui::Widgets& widget)
{
    widget.text(fmt::format("SPP: {}", mSPP));
    if (widget.button("Reset SPP"))
        mSPP = 0;
    if (widget.var("Max Bounce", mConfig.maxBounce, 1u, 16u))
        mSPP = 0;
}

} // namespace GSGI