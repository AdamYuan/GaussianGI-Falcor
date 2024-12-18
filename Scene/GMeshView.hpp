//
// Created by adamyuan on 12/17/24.
//

#ifndef GSGI_GMESHVIEW_HPP
#define GSGI_GMESHVIEW_HPP

#include <Falcor.h>
#include "GMesh.hpp"
#include "../Algorithm/MeshView.hpp"

using namespace Falcor;

namespace GSGI
{

struct GMeshVertexView
{
    const GMesh* pMesh;
    uint32_t vertexID;

    const float3& getPosition() const { return pMesh->vertices[vertexID].position; }
};

struct GMeshPrimitiveView
{
    const GMesh* pMesh;
    uint32_t primitiveID;

    GMeshVertexView getVertex(uint id) const
    {
        return GMeshVertexView{
            .pMesh = pMesh,
            .vertexID = pMesh->indices[primitiveID * 3 + id],
        };
    }
};

struct GMeshView
{
    const GMesh* pMesh;

    uint getPrimitiveCount() const { return pMesh->getPrimitiveCount(); }
    GMeshPrimitiveView getPrimitive(uint primitiveID) const
    {
        return GMeshPrimitiveView{
            .pMesh = pMesh,
            .primitiveID = primitiveID,
        };
    }

    static GMeshView make(const GMesh::Ptr& pMesh) { return GMeshView{.pMesh = pMesh.get()}; }
};

static_assert(Concepts::MeshView<GMeshView>);

} // namespace GSGI

#endif // GSGI_GMESHVIEW_HPP
