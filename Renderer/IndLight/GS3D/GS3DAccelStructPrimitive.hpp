//
// Created by adamyuan on 5/13/25.
//

#ifndef GSGI_GS3DACCELSTRUCTPRIMITIVETYPE_HPP
#define GSGI_GS3DACCELSTRUCTPRIMITIVETYPE_HPP

#include <Falcor.h>
#include "../../../Util/EnumUtil.hpp"
#include "../../../Algorithm/Icosahedron.hpp"
#include "../../../Algorithm/Octahedron.hpp"

using namespace Falcor;

namespace GSGI
{

enum class GS3DAccelStructPrimitiveType
{
    kIcosahedron,
    kOctahedron,
    GSGI_ENUM_COUNT
};

GSGI_ENUM_REGISTER(GS3DAccelStructPrimitiveType::kIcosahedron, Icosahedron, "Icosahedron", int);
GSGI_ENUM_REGISTER(GS3DAccelStructPrimitiveType::kOctahedron, Octahedron, "Octahedron", int);

inline void setGS3DAccelStructPrimitiveDefine(const ref<Program>& pProg, GS3DAccelStructPrimitiveType type)
{
    enumVisit(
        type,
        [&]<typename EnumInfo_T>(EnumInfo_T)
        { pProg->addDefine("AS_PRIM_TRIANGLE_COUNT", std::to_string(EnumInfo_T::Type::kTriangleCount)); }
    );
}

} // namespace GSGI

#endif // GSGI_GS3DACCELSTRUCTPRIMITIVETYPE_HPP
