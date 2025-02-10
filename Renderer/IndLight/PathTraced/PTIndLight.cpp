//
// Created by adamyuan on 12/15/24.
//

#include "PTIndLight.hpp"

#include "../../../Util/ShaderUtil.hpp"

namespace GSGI
{

PTIndLight::PTIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}

void PTIndLight::update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
{
    if (isSceneChanged)
    {
        mpStaticScene = pDefaultStaticScene;
        mSPP = 0;
    }
    if (!isActive)
        mSPP = 0;
}

void PTIndLight::draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture)
{
    if (bool isCameraChanged = mpStaticScene->getScene()->getCamera()->getData().viewProjMat != mViewProjMat,
        isLightingChanged = mpStaticScene->getScene()->getLighting()->getData() != mLightingData,
        isResolutionChanged = math::any(args.pVBuffer->getResolution() != mResolution);
        isCameraChanged || isLightingChanged || isResolutionChanged)
    {
        mViewProjMat = mpStaticScene->getScene()->getCamera()->getData().viewProjMat;
        mLightingData = mpStaticScene->getScene()->getLighting()->getData();
        mResolution = args.pVBuffer->getResolution();
        mSPP = 0;
    }

    if (!mpPass)
        mpPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/PathTraced/PTIndLight.cs.slang", "csMain");
    auto [prog, var] = getShaderProgVar(mpPass);
    var["gIndLight"] = pIndirectTexture;
    var["gSPP"] = mSPP;
    var["gMaxBounce"] = mConfig.maxBounce;
    mpStaticScene->bindRootShaderData(var);
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