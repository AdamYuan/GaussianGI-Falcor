//
// Created by adamyuan on 12/6/24.
//

#pragma once
#ifndef GSGI_GVBUFFER_HPP
#define GSGI_GVBUFFER_HPP

#include <Falcor.h>
#include <Core/Pass/RasterPass.h>
#include "../GDeviceObject.hpp"
#include "../Scene/GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

class GVBuffer final : public GDeviceObject<GVBuffer>
{
private:
    ref<RasterPass> mpRasterPass;
    ref<Fbo> mpFbo;
    ref<Texture> mpAlbedoTexture, mpHitTexture, mpDepthStencilTexture;

public:
    explicit GVBuffer(ref<Device> pDevice);

    uint2 getResolution() const;
    const auto& getAlbedoTexture() const { return mpAlbedoTexture; }
    const auto& getPrimitiveTexture() const { return mpHitTexture; }
    const ref<Texture>& getDepthStencilTexture() const { return mpDepthStencilTexture; }

    void draw(RenderContext* pRenderContext, const ref<Fbo>& pScreenFbo, const ref<GStaticScene>& pStaticScene);

    void bindShaderData(const ShaderVar& var) const;
};

} // namespace GSGI

#endif // GSGI_GVBUFFER_HPP
