//
// Created by adamyuan on 12/13/24.
//

#include "GIndLight.hpp"

namespace GSGI
{

GIndLight::GIndLight(const ref<GScene>& pScene) : GDeviceObject(pScene->getDevice())
{
    enumForEach<GIndLightType>([&]<typename EnumInfo_T>(EnumInfo_T)
                               { enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple) = make_ref<typename EnumInfo_T::Type>(pScene); });
}

void GIndLight::update(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, GIndLightType type)
{
    enumVisit(
        type,
        [&]<typename EnumInfo_T>(EnumInfo_T) { enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple)->update(pRenderContext, pStaticScene); }
    );
}

void GIndLight::draw(
    RenderContext* pRenderContext,
    const ref<GStaticScene>& pStaticScene,
    const GIndLightDrawArgs& args,
    const ref<Texture>& pIndirectTexture,
    GIndLightType type
)
{
    FALCOR_PROFILE(pRenderContext, "GIndLight::draw");
    enumVisit(
        type,
        [&]<typename EnumInfo_T>(EnumInfo_T)
        { enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple)->draw(pRenderContext, pStaticScene, args, pIndirectTexture); }
    );
}

void GIndLight::drawMisc(
    RenderContext* pRenderContext,
    const ref<GStaticScene>& pStaticScene,
    const ref<Fbo>& pTargetFbo,
    GIndLightType type
)
{
    FALCOR_PROFILE(pRenderContext, "GIndLight::drawMisc");
    enumVisit(
        type,
        [&]<typename EnumInfo_T>(EnumInfo_T)
        { enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple)->drawMisc(pRenderContext, pStaticScene, pTargetFbo); }
    );
}

void GIndLight::renderUIImpl(Gui::Widgets& widget, const EnumBitset<GIndLightType>& activeTypes)
{
    enumForEach<GIndLightType>(
        [&]<typename EnumInfo_T>(EnumInfo_T)
        {
            if (enumBitsetTest(activeTypes, EnumInfo_T::kValue))
            {
                if (auto g = widget.group(EnumInfo_T::kLabel, true))
                    enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple)->renderUI(g);
            }
        }
    );
}

} // namespace GSGI