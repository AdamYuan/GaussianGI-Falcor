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
    GLightingData mLightingData{};
    uint2 mResolution{};
    uint32_t mSPP = 0;

public:
    explicit PTIndLight(ref<Device> pDevice);
    void update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene);
    const auto& getStaticScene() const { return mpStaticScene; }
    void draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture);
};

} // namespace GSGI

#endif // GSGI_PTINDLIGHT_HPP
