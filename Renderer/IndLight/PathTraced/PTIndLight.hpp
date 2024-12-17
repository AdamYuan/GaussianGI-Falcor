//
// Created by adamyuan on 12/15/24.
//

#ifndef GSGI_PTINDLIGHT_HPP
#define GSGI_PTINDLIGHT_HPP

#include <Falcor.h>
#include "../../../Common/GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../GIndLightArgs.hpp"

using namespace Falcor;

namespace GSGI
{

class PTIndLight final : public GDeviceObject<PTIndLight>
{
private:
    ref<ComputePass> mpPass;
    ref<GStaticScene> mpStaticScene;

    uint32_t mSPP = 0;

    // Detect changes
    GLightingData mLightingData{};
    float4x4 mViewProjMat{};
    uint2 mResolution{};

public:
    explicit PTIndLight(ref<Device> pDevice);
    void update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene);
    const auto& getStaticScene() const { return mpStaticScene; }
    void draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture);
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_PTINDLIGHT_HPP
