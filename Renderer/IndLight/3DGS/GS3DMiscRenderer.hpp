//
// Created by adamyuan on 12/23/24.
//

#ifndef GSGI_GS3DMISCRENDERER_HPP
#define GSGI_GS3DMISCRENDERER_HPP

#include <Falcor.h>
#include "../../../Common/GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

class GS3DMiscRenderer final : public GDeviceObject<GS3DMiscRenderer>
{
private:
    ref<Vao> mpVao;
    // Point
    ref<RasterPass> mpPointPass;
    // Splat
    ref<ComputePass> mpSplatViewPass;
    ref<RasterPass> mpSplatDrawPass;

public:
    struct DrawArgs
    {
        const ref<GStaticScene>& pStaticScene;
        const ref<Buffer>& pSplatBuffer;
        uint splatsPerMesh;
    };
    explicit GS3DMiscRenderer(ref<Device> pDevice);
    void draw(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args);
};

} // namespace GSGI

#endif // GSGI_GS3DMISCRENDERER_HPP
