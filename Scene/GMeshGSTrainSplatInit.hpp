//
// Created by adamyuan on 2/5/25.
//

#ifndef GSGI_GMESHGSTRAINSPLATINIT_HPP
#define GSGI_GMESHGSTRAINSPLATINIT_HPP

#include <Falcor.h>
#include "GMesh.hpp"
#include "../Algorithm/MeshGSTrainer/MeshGSTrainer.hpp"

using namespace Falcor;

namespace GSGI
{

template<Concepts::MeshGSTrainTrait Trait_T>
struct GMeshGSTrainSplatInit
{
    ref<GMesh> pMesh;

    void initialize(
        RenderContext* pRenderContext,
        const typename MeshGSTrainer<Trait_T>::Resource& resource,
        uint splatCount,
        float initialScaleCoef = 0.5f
    ) const;
};

} // namespace GSGI

#endif // GSGI_GMESHGSTRAINSPLATINIT_HPP
