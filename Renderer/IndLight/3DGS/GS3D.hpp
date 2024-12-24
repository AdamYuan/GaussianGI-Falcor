//
// Created by adamyuan on 12/23/24.
//

#ifndef GSGI_GS3DCOMMON_HPP
#define GSGI_GS3DCOMMON_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct GS3DSplat
{
    float2 barycentrics;
    uint32_t primitiveID;
    float16_t4 rotate;
    float16_t2 scale;
};
static_assert(sizeof(GS3DSplat) == 6 * sizeof(uint32_t));

struct GS3DSplatView
{
    float16_t2 axis0, axis1;
    float16_t2 clipXY;
    uint32_t color;
};
static_assert(sizeof(GS3DSplatView) == 4 * sizeof(uint32_t));

} // namespace GSGI

#endif // GSGI_GS3DCOMMON_HPP
