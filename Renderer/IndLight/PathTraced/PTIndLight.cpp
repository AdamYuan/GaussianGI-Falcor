//
// Created by adamyuan on 12/15/24.
//

#include "PTIndLight.hpp"

#include "../../../Util/ShaderUtil.hpp"

namespace GSGI
{

PTIndLight::PTIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    mpPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/PathTraced/PTIndLight.cs.slang", "csMain");
}

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

    auto [prog, var] = getShaderProgVar(mpPass);
    var["gIndLight"] = pIndirectTexture;
    var["gSPP"] = mSPP;
    mpStaticScene->bindRootShaderData(var);
    args.pVBuffer->bindShaderData(var["gGVBuffer"]);
    args.pShadow->prepareProgram(prog, var["gGShadow"], args.shadowType);
    mpPass->execute(pRenderContext, mResolution.x, mResolution.y, 1);

    ++mSPP;
}

} // namespace GSGI