//
// Created by adamyuan on 12/17/24.
//

#ifndef GSGI_GS3DINDLIGHT_HPP
#define GSGI_GS3DINDLIGHT_HPP

#include <Falcor.h>
#include "../../../Common/GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../GIndLightArgs.hpp"
#include "GS3DMiscRenderer.hpp"

using namespace Falcor;

namespace GSGI
{

class GS3DIndLight final : public GDeviceObject<GS3DIndLight>
{
private:
    ref<GStaticScene> mpStaticScene;
    ref<GS3DMiscRenderer> mpMiscRenderer;
    ref<Buffer> mpSplatBuffer;

    struct Config
    {
        uint splatsPerMesh = 65536;

        bool operator==(const Config&) const = default;
    } mConfig = {};

public:
    explicit GS3DIndLight(ref<Device> pDevice);
    void update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene);
    const auto& getStaticScene() const { return mpStaticScene; }
    void draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture);
    void drawMisc(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo);
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_GS3DINDLIGHT_HPP
