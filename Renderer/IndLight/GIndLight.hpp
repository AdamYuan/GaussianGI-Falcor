//
// Created by adamyuan on 12/13/24.
//

#ifndef GSGI_GIRRADIANCE_HPP
#define GSGI_GIRRADIANCE_HPP

#include <Falcor.h>
#include "../../Common/EnumUtil.hpp"
#include "../../Common/GDeviceObject.hpp"
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
    void update(RenderContext* pRenderContext, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene);
    ref<GStaticScene> getStaticScene(GIndLightType type) const;
    void draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture, GIndLightType type);
};

} // namespace GSGI

#endif // GSGI_GIRRADIANCE_HPP
