//
// Created by adamyuan on 2/18/25.
//

#ifndef GSGI_BLASUTIL_HPP
#define GSGI_BLASUTIL_HPP

#include <Falcor.h>

using namespace Falcor;

namespace GSGI
{

struct BLASBuildInput
{
    std::vector<RtGeometryDesc> geomDescs;
};

struct BLASBuilder
{
    static std::vector<ref<RtAccelerationStructure>> build(RenderContext* pRenderContext, std::span<const BLASBuildInput> buildInputs);
    static ref<RtAccelerationStructure> build(RenderContext* pRenderContext, const BLASBuildInput& buildInput)
    {
        return build(pRenderContext, std::span{&buildInput, 1})[0];
    }
};

} // namespace GSGI

#endif // GSGI_BLASUTIL_HPP
