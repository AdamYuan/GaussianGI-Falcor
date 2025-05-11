//
// Created by adamyuan on 12/15/24.
//

#include "DDGIIndLight.hpp"

#include "../../../Util/ShaderUtil.hpp"

namespace GSGI
{

void DDGIIndLight::Grid::bindShaderData(const ShaderVar& var) const
{
    var["base"] = base;
    var["unit"] = unit;
    var["dim"] = dim;
}

DDGIIndLight::DDGIIndLight(const ref<GScene>& pScene) : GSceneObject(pScene) {}

void DDGIIndLight::updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene)
{
    if (isSceneChanged)
        mTick = 0;
}

void DDGIIndLight::draw(
    RenderContext* pRenderContext,
    const ref<GStaticScene>& pStaticScene,
    const GIndLightDrawArgs& args,
    const ref<Texture>& pIndirectTexture
)
{
    mConfig.gridDim = std::max(mConfig.gridDim, 2u);

    auto sceneExtent = pStaticScene->getBound().extent();
    Grid grid{
        .base = pStaticScene->getBound().minPoint,
        .unit = std::max(sceneExtent.x, std::max(sceneExtent.y, sceneExtent.z)) / float(mConfig.gridDim - 1u),
        .dim = mConfig.gridDim,
    };

    if (!mpProbeBuffers[0] || mpProbeBuffers[0]->getElementCount() != grid.getCount())
    {
        mTick = 0;
        for (auto& pBuf : mpProbeBuffers)
        {
            static constexpr std::size_t kProbeSize = 9 * sizeof(float16_t3);
            pBuf = getDevice()->createStructuredBuffer(kProbeSize, grid.getCount());
        }
    }

    auto resolution = args.pVBuffer->getResolution();

    if (!mpProbePass)
        mpProbePass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/DDGI/Probe.cs.slang", "csMain");
    if (!mpScreenPass)
        mpScreenPass = ComputePass::create(getDevice(), "GaussianGI/Renderer/IndLight/DDGI/Screen.cs.slang", "csMain");

    {
        FALCOR_PROFILE(pRenderContext, "probe");
        auto [prog, var] = getShaderProgVar(mpProbePass);
        var["gPrevProbes"] = mpProbeBuffers[0];
        var["gProbes"] = mpProbeBuffers[1];
        var["gTick"] = mTick;
        grid.bindShaderData(var["gGrid"]);
        args.pShadow->prepareProgram(prog, var["gGShadow"], args.shadowType);
        pStaticScene->bindRootShaderData(var);
        prog->addDefine("USE_FOLIAGE", mConfig.useFoliage ? "1" : "0");
        static constexpr uint32_t kRaysPerProbe = 32;
        mpProbePass->execute(pRenderContext, kRaysPerProbe * grid.dim, grid.dim, grid.dim);
        std::swap(mpProbeBuffers[0], mpProbeBuffers[1]);
    }

    {
        FALCOR_PROFILE(pRenderContext, "screen");
        auto [prog, var] = getShaderProgVar(mpScreenPass);
        var["gIndirect"] = pIndirectTexture;
        var["gResolution"] = resolution;
        args.pVBuffer->bindShaderData(var["gGVBuffer"]);
        var["gProbes"] = mpProbeBuffers[0];
        grid.bindShaderData(var["gGrid"]);
        pStaticScene->bindRootShaderData(var);
        mpScreenPass->execute(pRenderContext, resolution.x, resolution.y, 1);
    }

    ++mTick;
}

void DDGIIndLight::renderUIImpl(Gui::Widgets& widget)
{
    widget.text(fmt::format("Tick: {}", mTick));
    widget.var("Grid Dim", mConfig.gridDim, 2u, 64u);
    widget.checkbox("Use Foliage", mConfig.useFoliage);
    if (widget.button("Reset"))
        mTick = 0;
}

} // namespace GSGI