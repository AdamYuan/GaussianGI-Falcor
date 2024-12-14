//
// Created by adamyuan on 12/14/24.
//

#ifndef GSGI_GINDLIGHTARGS_HPP
#define GSGI_GINDLIGHTARGS_HPP

#include <Falcor.h>
#include "../GVBuffer.hpp"
#include "../Shadow/GShadow.hpp"

using namespace Falcor;

namespace GSGI
{

struct GIndLightDrawArgs
{
    const ref<GVBuffer>& pVBuffer;
    const ref<GShadow>& pShadow;
    GShadowType shadowType;
};

} // namespace GSGI

#endif // GSGI_GINDLIGHTARGS_HPP
