//
// Created by adamyuan on 12/11/24.
//

#ifndef GSGI_GSHADOWTYPE_HPP
#define GSGI_GSHADOWTYPE_HPP

#include <Falcor.h>
#include "../../Util/EnumUtil.hpp"
#include "StatelessShadow.hpp"
#include "PCF/PCFShadow.hpp"
#include "VSM/VSMShadow.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GShadowType
{
    kNone,
    kRayTraced,
    kPCF,
    kVSM,
    GSGI_ENUM_COUNT
};
struct GShadowTypeProperty
{
    const char* shaderName;
};
GSGI_ENUM_REGISTER(GShadowType::kNone, StatelessShadow, "None", GShadowTypeProperty, .shaderName = "noShadow");
GSGI_ENUM_REGISTER(GShadowType::kRayTraced, StatelessShadow, "Ray-traced", GShadowTypeProperty, .shaderName = "rtShadow");
GSGI_ENUM_REGISTER(GShadowType::kPCF, PCFShadow, "PCF", GShadowTypeProperty, .shaderName = "pcfShadow");
GSGI_ENUM_REGISTER(GShadowType::kVSM, VSMShadow, "VSM", GShadowTypeProperty, .shaderName = "vsmShadow");

} // namespace GSGI

#endif // GSGI_GSHADOWTYPE_HPP
