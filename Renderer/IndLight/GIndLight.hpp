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
    explicit GIndLight(ref<Device> pDevice);
    void update(
        RenderContext* pRenderContext,
        bool isSceneChanged,
        const ref<GStaticScene>& pDefaultStaticScene,
        const EnumBitset<GIndLightType>& activeTypes
    );
    ref<GStaticScene> getStaticScene(GIndLightType type) const;
    void draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture, GIndLightType type);
    void drawMisc(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, GIndLightType type);
    void renderUIImpl(Gui::Widgets& widget, const EnumBitset<GIndLightType>& activeTypes);
};

} // namespace GSGI

#endif // GSGI_GIRRADIANCE_HPP
