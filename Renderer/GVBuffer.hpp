//
// Created by adamyuan on 12/6/24.
//

#pragma once
#ifndef GSGI_GVBUFFER_HPP
#define GSGI_GVBUFFER_HPP

#include <Falcor.h>
#include <Core/Pass/RasterPass.h>
#include "../Common/GDeviceObject.hpp"
#include "../Scene/GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

class GVBuffer final : public GDeviceObject<GVBuffer>
{
private:
    ref<RasterPass> mpRasterPass;
    ref<Fbo> mpFbo;
    ref<Texture> mpAlbedoTexture, mpIDTexture;

public:
    explicit GVBuffer(ref<Device> pDevice);

    void draw(RenderContext* pRenderContext, const ref<Fbo> &pScreenFbo, const ref<GStaticScene> &pStaticScene);
};

} // namespace GSGI

#endif // GSGI_GVBUFFER_HPP
