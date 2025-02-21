//
// Created by adamyuan on 12/13/24.
//

#ifndef GSGI_GIRRADIANCE_HPP
#define GSGI_GIRRADIANCE_HPP

#include <Falcor.h>
#include "../../Util/EnumUtil.hpp"
#include "../../GDeviceObject.hpp"
#include "GIndLightType.hpp"
#include "GIndLightArgs.hpp"

using namespace Falcor;

namespace GSGI
{

class GIndLight final : public GDeviceObject<GIndLight>
{
private:
    EnumRefTuple<GIndLightType> mpIndirectTuple;

public:
    explicit GIndLight(const ref<GScene>& pScene);
    ~GIndLight() override = default;

    void update(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, GIndLightType type);
    void draw(
        RenderContext* pRenderContext,
        const ref<GStaticScene>& pStaticScene,
        const GIndLightDrawArgs& args,
        const ref<Texture>& pIndirectTexture,
        GIndLightType type
    );
    void drawMisc(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, const ref<Fbo>& pTargetFbo, GIndLightType type);
    void renderUIImpl(Gui::Widgets& widget, const EnumBitset<GIndLightType>& activeTypes);
};

} // namespace GSGI

#endif // GSGI_GIRRADIANCE_HPP
