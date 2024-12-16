//
// Created by adamyuan on 12/16/24.
//

#ifndef GSGI_GRENDERERVIEWTYPE_HPP
#define GSGI_GRENDERERVIEWTYPE_HPP

#include <Falcor.h>
#include "../Util/EnumUtil.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GRendererViewType
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

struct GRendererViewTypeProperty
{
    const char* varName;
    bool gammaCorrection;
};

GSGI_ENUM_REGISTER(
    GRendererViewType::kRadiance,
    void,
    "Radiance",
    GRendererViewTypeProperty,
    .varName = "radiance",
    .gammaCorrection = true
);
GSGI_ENUM_REGISTER(
    GRendererViewType::kDirectRadiance,
    void,
    "Direct Radiance",
    GRendererViewTypeProperty,
    .varName = "directRadiance",
    .gammaCorrection = true
);
GSGI_ENUM_REGISTER(
    GRendererViewType::kDirectLight,
    void,
    "Direct Light",
    GRendererViewTypeProperty,
    .varName = "direct",
    .gammaCorrection = true
);
GSGI_ENUM_REGISTER(
    GRendererViewType::kIndirectLight,
    void,
    "Indirect Light",
    GRendererViewTypeProperty,
    .varName = "indirect",
    .gammaCorrection = true
);
GSGI_ENUM_REGISTER(
    GRendererViewType::kAlbedo, //
    void,
    "Albedo",
    GRendererViewTypeProperty,
    .varName = "albedo",
    .gammaCorrection = false
);
GSGI_ENUM_REGISTER(
    GRendererViewType::kNormal,
    void,
    "Normal",
    GRendererViewTypeProperty,
    .varName = "clampedNormal",
    .gammaCorrection = false
);
GSGI_ENUM_REGISTER(
    GRendererViewType::kShadow, //
    void,
    "Shadow",
    GRendererViewTypeProperty,
    .varName = "shadow",
    .gammaCorrection = false
);

} // namespace GSGI

#endif // GSGI_GRENDERERVIEWTYPE_HPP
