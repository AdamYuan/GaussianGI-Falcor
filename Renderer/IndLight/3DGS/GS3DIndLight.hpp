//
// Created by adamyuan on 12/17/24.
//

#ifndef GSGI_GS3DINDLIGHT_HPP
#define GSGI_GS3DINDLIGHT_HPP

#include <Falcor.h>
#include "../../../Common/GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../GIndLightArgs.hpp"

using namespace Falcor;

namespace GSGI
{

class GS3DIndLight final : public GDeviceObject<GS3DIndLight>
{
private:
    ref<GStaticScene> mpStaticScene;

    ref<VertexLayout> mpPointVertexLayout;
    ref<RasterPass> mpPointPass;
    ref<Buffer> mpPointBuffer;
    ref<Vao> mpPointVao;

    struct
    {
        uint splatsPerMesh = 65536;
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
