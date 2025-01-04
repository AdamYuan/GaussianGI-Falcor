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
    const GMesh& mesh;
    uint32_t vertexID;

    const float3& getPosition() const { return mesh.getData().vertices[vertexID].position; }
};

struct GMeshPrimitiveView
{
    const GMesh& mesh;
    uint32_t primitiveID;

    GMeshVertexView getVertex(uint id) const
    {
        return GMeshVertexView{
            .mesh = mesh,
            .vertexID = mesh.getData().indices[primitiveID * 3 + id],
        };
    }
};

struct GMeshView
{
    const GMesh& mesh;

    explicit GMeshView(const ref<GMesh>& pMesh) : mesh{*pMesh} {}

    uint getPrimitiveCount() const { return mesh.getPrimitiveCount(); }
    GMeshPrimitiveView getPrimitive(uint primitiveID) const
    {
        return GMeshPrimitiveView{
            .mesh = mesh,
            .primitiveID = primitiveID,
        };
    }
    const AABB& getAABB() const { return mesh.getBound(); }
};

static_assert(Concepts::MeshView<GMeshView>);

} // namespace GSGI

#endif // GSGI_GMESHVIEW_HPP
