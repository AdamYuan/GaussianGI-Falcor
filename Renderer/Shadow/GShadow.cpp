//
// Created by adamyuan on 12/11/24.
//

#include "GShadow.hpp"

namespace GSGI
{

GShadow::GShadow(const ref<GScene>& pScene) : GDeviceObject(pScene->getDevice())
{
    enumForEach<GShadowType>([&]<typename EnumInfo_T>(EnumInfo_T)
                             { enumTupleGet<EnumInfo_T::kValue>(mpShadowTuple) = make_ref<typename EnumInfo_T::Type>(pScene); });
}

void GShadow::update(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene)
{
    bool isLightChanged;

    {
        float3 lightDir = pStaticScene->getLighting()->getData().direction;
        isLightChanged = math::any(lightDir != mLightDirection);
        mLightDirection = lightDir;
    }

    enumForEach<GShadowType>([&]<typename EnumInfo_T>(EnumInfo_T)
                             { enumTupleGet<EnumInfo_T::kValue>(mpShadowTuple)->update(pRenderContext, pStaticScene, isLightChanged); });
}

void GShadow::prepareProgram(const ref<Program>& pProgram, const ShaderVar& var, GShadowType type) const
{
    enumVisit(
        type,
        [&]<typename EnumInfo_T>(EnumInfo_T)
        {
            const auto& shaderName = EnumInfo_T::kProperty.shaderName;
            pProgram->addDefine("GSHADOW_IDENTIFIER", shaderName);
            enumTupleGet<EnumInfo_T::kValue>(mpShadowTuple)->bindShaderData(var[shaderName]);
        }
    );
}

void GShadow::renderUIImpl(Gui::Widgets& widget, const EnumBitset<GShadowType>& activeTypes)
{
    enumForEach<GShadowType>(
        [&]<typename EnumInfo_T>(EnumInfo_T)
        {
            if (enumBitsetTest(activeTypes, EnumInfo_T::kValue))
            {
                if (auto g = widget.group(EnumInfo_T::kLabel, true))
                    enumTupleGet<EnumInfo_T::kValue>(mpShadowTuple)->renderUI(g);
            }
        }
    );
}

} // namespace GSGI