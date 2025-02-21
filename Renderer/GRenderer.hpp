//
// Created by adamyuan on 12/5/24.
//

#pragma once
#ifndef GSGI_GRENDERER_HPP
#define GSGI_GRENDERER_HPP

#include <Falcor.h>
#include "../Scene/GSceneObject.hpp"
#include "../Scene/GStaticScene.hpp"
#include "GRendererViewType.hpp"
#include "GVBuffer.hpp"
#include "Shadow/GShadow.hpp"
#include "IndLight/GIndLight.hpp"

using namespace Falcor;

namespace GSGI
{

class GRenderer final : public GSceneObject<GRenderer>
{
private:
    ref<GStaticScene> mpStaticScene;
    ref<GShadow> mpShadow;
    ref<GIndLight> mpIndirectLight;

    ref<GVBuffer> mpVBuffer;

    ref<ComputePass> mpPass;
    ref<Texture> mpTargetTexture, mpIndLightTexture;

    struct
    {
        GRendererViewType viewType = GRendererViewType::kRadiance;
        bool drawMisc = false;
        GShadowType directShadowType = GShadowType::kRayTraced;
        GShadowType indirectShadowType = GShadowType::kRayTraced;
        GIndLightType indirectLightType = GIndLightType::kNone;
    } mConfig = {};

public:
    explicit GRenderer(const ref<GScene>& pScene);
    ~GRenderer() override = default;

    void updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo);
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_GRENDERER_HPP
