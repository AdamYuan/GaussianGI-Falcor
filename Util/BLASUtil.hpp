//
// Created by adamyuan on 2/18/25.
//

#ifndef GSGI_BLASUTIL_HPP
#define GSGI_BLASUTIL_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct BLASBuildDesc
{
    std::vector<RtGeometryDesc> geomDescs;
};

struct BLASBuilder
{
    static std::vector<ref<RtAccelerationStructure>> build(RenderContext* pRenderContext, std::span<const BLASBuildDesc> buildDescs);
    static ref<RtAccelerationStructure> build(RenderContext* pRenderContext, const BLASBuildDesc& buildDesc)
    {
        return build(pRenderContext, std::span{&buildDesc, 1})[0];
    }
};

} // namespace GSGI

#endif // GSGI_BLASUTIL_HPP
