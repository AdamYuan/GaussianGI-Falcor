//
// Created by adamyuan on 5/11/25.
//

#ifndef GSGI_GS3DINDLIGHTSPLATPRIMITIVE_HPP
#define GSGI_GS3DINDLIGHTSPLATPRIMITIVE_HPP

#include <Falcor.h>
#include "../../../Util/EnumUtil.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GS3DPrimitiveType
{
    kNaive,
    kGSPP,
    kRay,
    GSGI_ENUM_COUNT
};
struct GS3DPrimitiveTypeProperty
{
    const char* typeName;
};
GSGI_ENUM_REGISTER(GS3DPrimitiveType::kNaive, void, "Naive", GS3DPrimitiveTypeProperty, .typeName = "GS_PRIMITIVE_TYPE_NAIVE");
GSGI_ENUM_REGISTER(GS3DPrimitiveType::kGSPP, void, "GS++", GS3DPrimitiveTypeProperty, .typeName = "GS_PRIMITIVE_TYPE_GSPP");
GSGI_ENUM_REGISTER(GS3DPrimitiveType::kRay, void, "Ray", GS3DPrimitiveTypeProperty, .typeName = "GS_PRIMITIVE_TYPE_RAY");

inline void setGS3DPrimitiveTypeDefine(const ref<Program>& pProg, GS3DPrimitiveType type)
{
    enumVisit(type, [&]<typename EnumInfo_T>(EnumInfo_T) { pProg->addDefine("GS_PRIMITIVE_TYPE", EnumInfo_T::kProperty.typeName); });
}

} // namespace GSGI

#endif // GSGI_GS3DINDLIGHTSPLATPRIMITIVE_HPP
