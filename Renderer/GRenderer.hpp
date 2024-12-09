//
// Created by adamyuan on 12/5/24.
//

#pragma once
#ifndef GSGI_GRENDERER_HPP
#define GSGI_GRENDERER_HPP

#include <Falcor.h>
#include "../Scene/GSceneObject.hpp"
#include "../Scene/GStaticScene.hpp"
#include "GVBuffer.hpp"

using namespace Falcor;

namespace GSGI
{

class GRenderer final : public GSceneObject<GRenderer>
{
private:
    ref<GStaticScene> mpDefaultStaticScene;
    ref<GVBuffer> mpVBuffer;

    ref<ComputePass> mpPass;
    ref<Texture> mpTexture;

public:
    explicit GRenderer(const ref<GScene>& pScene);
    ~GRenderer() override = default;

    const auto& getTexture() const { return mpTexture; }

    void updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo);
};

} // namespace GSGI

#endif // GSGI_GRENDERER_HPP
