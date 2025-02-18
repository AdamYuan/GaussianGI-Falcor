//
// Created by adamyuan on 2/18/25.
//

#ifndef GSGI_TLASUTIL_HPP
#define GSGI_TLASUTIL_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct TLASBuildDesc
{
    std::vector<RtInstanceDesc> instanceDescs;
};

struct TLASBuilder
{
    static ref<RtAccelerationStructure> build(RenderContext* pRenderContext, const TLASBuildDesc& buildDesc);
};

} // namespace GSGI

#endif // GSGI_TLASUTIL_HPP
