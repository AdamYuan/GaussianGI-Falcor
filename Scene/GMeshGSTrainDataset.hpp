//
// Created by adamyuan on 1/12/25.
//

#ifndef GSGI_GMESHGSTRAINDATASET_HPP
#define GSGI_GMESHGSTRAINDATASET_HPP

#include <Falcor.h>
#include "GMesh.hpp"
#include "../Algorithm/MeshGSTrainer/MeshGSTrainer.hpp"

using namespace Falcor;

namespace GSGI
{

template<MeshGSTrainType TrainType_V>
struct GMeshGSTrainDataset
{
    GMesh& mesh;
    void generate(RenderContext* pRenderContext, const MeshGSTrainMeshRT<TrainType_V>& rt, const MeshGSTrainCamera& camera) const;
};

} // namespace GSGI

#endif // GSGI_GMESHGSTRAINDATASET_HPP
