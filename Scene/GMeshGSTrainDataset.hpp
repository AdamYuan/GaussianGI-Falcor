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

template<MeshGSTrainType TrainType_V, typename RandGen_T = std::mt19937>
struct GMeshGSTrainDataset
{
    ref<GMesh> pMesh;
    RandGen_T randGen;
    struct
    {
        float eyeExtent = 2.0f;
    } config = {};
    MeshGSTrainCamera nextCamera(const MeshGSTrainMeshRT<TrainType_V>& rt);
    void draw(RenderContext* pRenderContext, const MeshGSTrainMeshRT<TrainType_V>& rt, const MeshGSTrainCamera& camera) const;
};

} // namespace GSGI

#endif // GSGI_GMESHGSTRAINDATASET_HPP
