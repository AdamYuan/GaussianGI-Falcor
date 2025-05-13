//
// Created by adamyuan on 12/23/24.
//

#ifndef GSGI_GS3DMISCRENDERER_HPP
#define GSGI_GS3DMISCRENDERER_HPP

#include <Falcor.h>
#include "../../../../Util/EnumUtil.hpp"
#include "../../../../GDeviceObject.hpp"
#include "../../../../Scene/GStaticScene.hpp"
#include "../../../../Algorithm/DeviceSort/DeviceSorter.hpp"
#include "../GS3DIndLightSplat.hpp"
#include "../GS3DIndLightSplatPrimitive.hpp"
#include "../GS3DAccelStructPrimitive.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GS3DMiscType
{
    kPoint,
    kSplat,
    kTracedSplat,
    GSGI_ENUM_COUNT
};
GSGI_ENUM_REGISTER(GS3DMiscType::kPoint, void, "Point", int);
GSGI_ENUM_REGISTER(GS3DMiscType::kSplat, void, "Splat", int);
GSGI_ENUM_REGISTER(GS3DMiscType::kTracedSplat, void, "Traced-Splat", int);

enum class GS3DMiscColorType
{
    kRadiance,
    kDirectRadiance,
    kDirectLight,
    kIndirectLight,
    kAlbedo,
    kNormal,
    kShadow,
    GSGI_ENUM_COUNT
};
struct GS3DMiscColorTypeProperty
{
    const char* varName;
};
GSGI_ENUM_REGISTER(GS3DMiscColorType::kRadiance, void, "Radiance", GS3DMiscColorTypeProperty, .varName = "radiance");
GSGI_ENUM_REGISTER(GS3DMiscColorType::kDirectRadiance, void, "Direct Radiance", GS3DMiscColorTypeProperty, .varName = "directRadiance");
GSGI_ENUM_REGISTER(GS3DMiscColorType::kDirectLight, void, "Direct Light", GS3DMiscColorTypeProperty, .varName = "directLight");
GSGI_ENUM_REGISTER(GS3DMiscColorType::kIndirectLight, void, "Indirect Light", GS3DMiscColorTypeProperty, .varName = "indirectLight");
GSGI_ENUM_REGISTER(GS3DMiscColorType::kAlbedo, void, "Albedo", GS3DMiscColorTypeProperty, .varName = "albedo");
GSGI_ENUM_REGISTER(GS3DMiscColorType::kNormal, void, "Normal", GS3DMiscColorTypeProperty, .varName = "normal");
GSGI_ENUM_REGISTER(GS3DMiscColorType::kShadow, void, "Shadow", GS3DMiscColorTypeProperty, .varName = "shadow");

class GS3DMiscRenderer final : public GDeviceObject<GS3DMiscRenderer>
{
public:
    struct DrawArgs
    {
        const ref<GStaticScene>& pStaticScene;
        const GS3DIndLightInstancedSplatBuffer& instancedSplatBuffer;
        const ref<RtAccelerationStructure>& pSplatTLAS;
        const ref<Buffer>& pSplatShadowBuffer;
        const ref<Buffer>& pSplatProbeBuffer;
        GS3DAccelStructPrimitiveType accelStructPrimitiveType;
    };

private:
    // Point
    ref<RasterPass> mpPointPass;
    // Splat
    ref<ComputePass> mpSplatViewPass;
    ref<RasterPass> mpSplatDrawPass;
    ref<Buffer> mpSplatViewSortKeyBuffer, mpSplatViewSortPayloadBuffer;
    ref<Buffer> mpSplatViewDrawArgBuffer;
    DeviceSorter<DeviceSortDispatchType::kIndirect> mSplatViewSorter;
    DeviceSortResource<DeviceSortDispatchType::kIndirect> mSplatViewSortResource;
    // Splat-RT
    struct
    {
        ref<Program> pProgram;
        ref<RtBindingTable> pBindingTable;
        ref<RtProgramVars> pVars;
    } mSplatTracePass;
    ref<Texture> mpSplatTraceColorTexture;

    struct
    {
        GS3DMiscType type = GS3DMiscType::kSplat;
        GS3DMiscColorType colorType = GS3DMiscColorType::kRadiance;
        GS3DPrimitiveType primitiveType = GS3DPrimitiveType::kGSPP;
    } mConfig = {};

    void drawPoints(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args);
    void drawSplats(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args);
    void drawTracedSplats(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args);

public:
    explicit GS3DMiscRenderer(const ref<Device>& pDevice);
    void draw(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args);
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_GS3DMISCRENDERER_HPP
