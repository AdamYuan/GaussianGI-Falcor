//
// Created by adamyuan on 12/11/24.
//

#ifndef GSGI_GSHADOWTYPE_HPP
#define GSGI_GSHADOWTYPE_HPP

#include <Falcor.h>
#include "../../Common/EnumUtil.hpp"
#include "StatelessShadow.hpp"
#include "PCF/PCFShadow.hpp"
#include "EVSM/EVSMShadow.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GShadowType
{
    kNone,
    kRayTraced,
    kPCF,
    kEVSM,
    GSGI_ENUM_COUNT
};
GSGI_ENUM_REGISTER(GShadowType::kNone, StatelessShadow, "None", "noShadow");
GSGI_ENUM_REGISTER(GShadowType::kRayTraced, StatelessShadow, "Ray-traced", "rtShadow");
GSGI_ENUM_REGISTER(GShadowType::kPCF, PCFShadow, "PCF", "pcfShadow");
GSGI_ENUM_REGISTER(GShadowType::kEVSM, EVSMShadow, "EVSM", "evsmShadow");

} // namespace GSGI

#endif // GSGI_GSHADOWTYPE_HPP
