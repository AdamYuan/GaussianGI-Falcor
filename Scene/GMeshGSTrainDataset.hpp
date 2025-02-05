//
// Created by adamyuan on 1/12/25.
//

#ifndef GSGI_GMESHGSTRAINDATASET_HPP
#define GSGI_GMESHGSTRAINDATASET_HPP

#include <Falcor.h>
#include "GMesh.hpp"
#include "../Algorithm/MeshGSTrainer/MeshGSTrainer.hpp"
#include <random>

using namespace Falcor;

namespace GSGI
{

template<Concepts::MeshGSTrainTrait Trait_T>
struct GMeshGSTrainDataset
{
    ref<GMesh> pMesh;
    std::mt19937 randGen;
    struct
    {
        float eyeExtent = 2.0f;
    } config = {};

    void generate(RenderContext* pRenderContext, typename MeshGSTrainer<Trait_T>::Data& data, uint2 resolution, bool generateCamera);
};

} // namespace GSGI

#endif // GSGI_GMESHGSTRAINDATASET_HPP
