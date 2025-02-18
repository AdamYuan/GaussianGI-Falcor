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

struct BLASBuildResult
{
    std::vector<ref<RtAccelerationStructure>> pBLASs;
};

BLASBuildResult buildBLAS(RenderContext* pRenderContext, std::span<const BLASBuildInput> buildInputs);

} // namespace GSGI

#endif // GSGI_BLASUTIL_HPP
