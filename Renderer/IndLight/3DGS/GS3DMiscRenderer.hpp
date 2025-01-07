//
// Created by adamyuan on 12/23/24.
//

#ifndef GSGI_GS3DMISCRENDERER_HPP
#define GSGI_GS3DMISCRENDERER_HPP

#include <Falcor.h>
#include "../../../Util/EnumUtil.hpp"
#include "../../../GDeviceObject.hpp"
#include "../../../Scene/GStaticScene.hpp"
#include "../../../Algorithm/DeviceSort/DeviceSorter.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GS3DMiscType
{
    kPoint,
    kSplat,
    GSGI_ENUM_COUNT
};
GSGI_ENUM_REGISTER(GS3DMiscType::kPoint, void, "Point", int);
GSGI_ENUM_REGISTER(GS3DMiscType::kSplat, void, "Splat", int);

class GS3DMiscRenderer final : public GDeviceObject<GS3DMiscRenderer>
{
private:
    // Point
    ref<RasterPass> mpPointPass;
    // Splat
    ref<ComputePass> mpSplatViewPass;
    ref<RasterPass> mpSplatDrawPass;
    ref<Buffer> mpSplatViewBuffer, mpSplatViewSortKeyBuffer, mpSplatViewSortPayloadBuffer;
    ref<Buffer> mpSplatViewDrawArgBuffer;
    DeviceSorter<DeviceSortDispatchType::kIndirect> mSplatViewSorter;
    DeviceSortResource<DeviceSortDispatchType::kIndirect> mSplatViewSortResource;
    uint mSplatViewCount{};

    struct
    {
        GS3DMiscType type = GS3DMiscType::kSplat;
        float splatOpacity = 1.0f;
    } mConfig = {};

public:
    struct DrawArgs
    {
        const ref<GStaticScene>& pStaticScene;
        const ref<Buffer>& pSplatBuffer;
        uint splatsPerMesh;
    };
    explicit GS3DMiscRenderer(const ref<Device>& pDevice);
    void draw(RenderContext* pRenderContext, const ref<Fbo>& pTargetFbo, const DrawArgs& args);
    void renderUIImpl(Gui::Widgets& widget);
};

} // namespace GSGI

#endif // GSGI_GS3DMISCRENDERER_HPP
