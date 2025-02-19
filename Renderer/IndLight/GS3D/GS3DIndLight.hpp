//
// Created by adamyuan on 12/17/24.
//

#ifndef GSGI_GS3DINDLIGHT_HPP
#define GSGI_GS3DINDLIGHT_HPP

#include <Falcor.h>
#include "../../../GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../GIndLightArgs.hpp"
#include "Misc/GS3DMiscRenderer.hpp"
#include "GS3DIndLightAlgo.hpp"

using namespace Falcor;

namespace GSGI
{

class GS3DIndLight final : public GDeviceObject<GS3DIndLight>
{
private:
    ref<GStaticScene> mpStaticScene;
    ref<GS3DMiscRenderer> mpMiscRenderer;
    GS3DIndLightInstancedSplatBuffer mInstancedSplatBuffer;
    std::vector<ref<RtAccelerationStructure>> mpSplatBLASs;
    ref<RtAccelerationStructure> mpSplatTLAS;

    struct
    {
        ref<ComputePass> pCullPass, pBlendPass;
        ref<RasterPass> pDrawPass;
        // DeviceSorter<DeviceSortDispatchType::kIndirect> splatViewSorter;
        // DeviceSortResource<DeviceSortDispatchType::kIndirect> splatViewSortResource;
        // ref<Buffer> pSplatViewBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer;
        ref<Buffer> pSplatIDBuffer;
        ref<Buffer> pSplatDrawArgBuffer;
        ref<Fbo> pSplatFbo;
    } mDrawResource{};

    struct Config
    {
        bool operator==(const Config&) const = default;
    } mConfig = {};

    void updateDrawResource(const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture);
    void onSceneChanged();

public:
    explicit GS3DIndLight(ref<Device> pDevice);
    void update(RenderContext* pRenderContext, bool isActive, bool isSceneChanged, const ref<GStaticScene>& pDefaultStaticScene);
    const auto& getStaticScene() const { return mpStaticScene; }
    void draw(RenderContext* pRenderContext, const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture);
    void drawMisc(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo);
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_GS3DINDLIGHT_HPP
