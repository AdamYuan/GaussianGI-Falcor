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

class GS3DIndLight final : public GSceneObject<GS3DIndLight>
{
private:
    ref<GS3DMiscRenderer> mpMiscRenderer;
    GS3DIndLightInstancedSplatBuffer mInstancedSplatBuffer;
    std::vector<ref<RtAccelerationStructure>> mpSplatBLASs;
    ref<RtAccelerationStructure> mpSplatTLAS;

    struct
    {
        ref<ComputePass> pCullPass, pBlendPass, pShadowPass, pProbePass, pZNormalPass;
        ref<RasterPass> pDrawPass;
        struct
        {
            ref<Program> pProgram;
            ref<RtBindingTable> pBindingTable;
            ref<RtProgramVars> pVars;
        } traceShadowPass;
        // DeviceSorter<DeviceSortDispatchType::kIndirect> splatViewSorter;
        // DeviceSortResource<DeviceSortDispatchType::kIndirect> splatViewSortResource;
        // ref<Buffer> pSplatViewBuffer, pSplatViewSortKeyBuffer, pSplatViewSortPayloadBuffer;
        ref<Buffer> pSplatIDBuffer, pSplatShadowBuffer;
        ref<Buffer> pSrcSplatProbeBuffer, pDstSplatProbeBuffer;
        ref<Buffer> pSplatDrawArgBuffer;
        ref<Texture> pZNormalTexture;
        ref<Fbo> pSplatFbo;
        uint32_t probeTick{};
    } mDrawResource{};

    struct Config
    {
        bool useTracedShadow = true;
        bool vndfSample = true;
        bool useStencil = false;
    } mConfig = {}, mPrevConfig = {};

    float3 mPrevTracedShadowDirection{};

    void updateDrawResource(const GIndLightDrawArgs& args, const ref<Texture>& pIndirectTexture);
    static void preprocessMeshSplats(std::vector<GS3DIndLightSplat>& meshSplats);
    void onSceneChanged(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene);

public:
    explicit GS3DIndLight(const ref<GScene>& pScene);
    ~GS3DIndLight() override = default;

    void updateImpl(bool isSceneChanged, RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene);
    void draw(
        RenderContext* pRenderContext,
        const ref<GStaticScene>& pStaticScene,
        const GIndLightDrawArgs& args,
        const ref<Texture>& pIndirectTexture
    );
    void drawMisc(RenderContext* pRenderContext, const ref<GStaticScene>& pStaticScene, const ref<Fbo>& pTargetFbo);
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_GS3DINDLIGHT_HPP
