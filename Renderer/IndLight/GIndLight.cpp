//
// Created by adamyuan on 12/13/24.
//

#include "GIndLight.hpp"

namespace GSGI
{

GIndLight::GIndLight(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    enumForEach<GIndLightType>([&]<typename EnumInfo_T>(EnumInfo_T)
                               { enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple) = make_ref<typename EnumInfo_T::Type>(getDevice()); });
}

void GIndLight::update(RenderContext* pRenderContext, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene)
{
    enumForEach<GIndLightType>(
        [&]<typename EnumInfo_T>(EnumInfo_T)
        { enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple)->update(pRenderContext, isSceneChanged, pDefaultStaticScene); }
    );
}

ref<GStaticScene> GIndLight::getStaticScene(GIndLightType type) const
{
    return enumVisit(
        type, [&]<typename EnumInfo_T>(EnumInfo_T) { return enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple)->getStaticScene(); }
    );
}

void GIndLight::draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture, GIndLightType type)
{
    enumVisit(
        type,
        [&]<typename EnumInfo_T>(EnumInfo_T)
        { enumTupleGet<EnumInfo_T::kValue>(mpIndirectTuple)->draw(pRenderContext, args, pIndirectTexture); }
    );
}

} // namespace GSGI