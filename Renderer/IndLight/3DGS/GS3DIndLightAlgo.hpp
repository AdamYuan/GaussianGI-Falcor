//
// Created by adamyuan on 2/8/25.
//

#ifndef GSGI_GS3DINDLIGHTALGO_HPP
#define GSGI_GS3DINDLIGHTALGO_HPP

#include <Falcor.h>
#include "GS3DIndLightSplat.hpp"

using namespace Falcor;

namespace GSGI
{

struct GS3DIndLightAlgo
{
    static std::vector<GS3DIndLightSplat> getSplatsFromMeshFallback(const ref<GMesh>& pMesh, uint splatCount);
};

} // namespace GSGI

#endif // GSGI_GS3DINDLIGHTALGO_HPP
