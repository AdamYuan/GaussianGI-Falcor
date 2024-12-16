//
// Created by adamyuan on 12/14/24.
//

#ifndef GSGI_GINDIRECTTYPE_HPP
#define GSGI_GINDIRECTTYPE_HPP

#include <Falcor.h>
#include "../../Common/EnumUtil.hpp"
#include "NoIndLight.hpp"
#include "PathTraced/PTIndLight.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GIndLightType
{
    kNone,
    kPathTraced,
    k3DGS,
    kDDGI,
    GSGI_ENUM_COUNT
};

struct GIndLightTypeProperty
{};

GSGI_ENUM_REGISTER(GIndLightType::kNone, NoIndLight, "None", GIndLightTypeProperty);
GSGI_ENUM_REGISTER(GIndLightType::kPathTraced, PTIndLight, "Path-traced", GIndLightTypeProperty);
GSGI_ENUM_REGISTER(GIndLightType::k3DGS, NoIndLight, "3DGS", GIndLightTypeProperty);
GSGI_ENUM_REGISTER(GIndLightType::kDDGI, NoIndLight, "DDGI", GIndLightTypeProperty);

} // namespace GSGI

#endif // GSGI_GINDIRECTTYPE_HPP