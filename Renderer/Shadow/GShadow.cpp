//
// Created by adamyuan on 12/11/24.
//

#include "GShadow.hpp"

namespace GSGI
{

GShadow::GShadow(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    enumForEach<GShadowType>([&]<typename EnumInfo_T>(EnumInfo_T)
                             { enumTupleGet<EnumInfo_T::kValue>(mpShadowTuple) = make_ref<typename EnumInfo_T::Type>(getDevice()); });
}

void GShadow::update(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene)
{
    bool isStaticSceneChanged, isLightChanged;

    {
        float3 lightDir = pStaticScene->getScene()->getLighting()->getData().direction;
        isLightChanged = math::any(lightDir != mLightDirection);
        isStaticSceneChanged = pStaticScene != mpStaticScene;
        mpStaticScene = pStaticScene;
        mLightDirection = lightDir;
    }

    enumForEach<GShadowType>(
        [&]<typename EnumInfo_T>(EnumInfo_T)
        { enumTupleGet<EnumInfo_T::kValue>(mpShadowTuple)->update(pRenderContext, isStaticSceneChanged, isLightChanged, mpStaticScene); }
    );
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

} // namespace GSGI