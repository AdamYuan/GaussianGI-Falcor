//
// Created by adamyuan on 12/11/24.
//
#include "GMesh.hpp"

#include "GMesh.slangh"

namespace GSGI
{

static_assert(GMesh::kMaxTextureCount == GMESH_MAX_TEXTURE_COUNT);

ref<VertexLayout> GMesh::createVertexLayout()
{
    auto vertexBufferLayout = VertexBufferLayout::create();
    vertexBufferLayout->addElement(GMESH_VERTEX_POSITION_NAME, 0, ResourceFormat::RGB32Float, 1, GMESH_VERTEX_POSITION_LOC);
    vertexBufferLayout->addElement(GMESH_VERTEX_NORMAL_NAME, 3 * sizeof(float), ResourceFormat::RGB32Float, 1, GMESH_VERTEX_NORMAL_LOC);
    vertexBufferLayout->addElement(GMESH_VERTEX_TEXCOORD_NAME, 6 * sizeof(float), ResourceFormat::RG32Float, 1, GMESH_VERTEX_TEXCOORD_LOC);
    auto vertexLayout = VertexLayout::create();
    vertexLayout->addBufferLayout(0, std::move(vertexBufferLayout));
    return vertexLayout;
}

std::vector<RtGeometryDesc> GMesh::getRTGeometryDescs(
    DeviceAddress transform3x4Addr,
    DeviceAddress indexBufferAddr,
    DeviceAddress vertexBufferAddr
) const
{
    std::vector<RtGeometryDesc> geomDescs;

    RtGeometryDesc geomDesc{};
    geomDesc.type = RtGeometryType::Triangles;
    geomDesc.content.triangles = {
        .transform3x4 = transform3x4Addr,
        .indexFormat = ResourceFormat::R32Uint,
        .vertexFormat = ResourceFormat::RGB32Float,
        .indexCount = 0,
        .vertexCount = 0, // Seems to work (at least on Vulkan)
        .indexData = indexBufferAddr,
        .vertexData = vertexBufferAddr,
        .vertexStride = sizeof(Vertex),
    };

    for (uint primitiveCount = getPrimitiveCount(), primitiveID = 0; primitiveID < primitiveCount; ++primitiveID)
    {
        RtGeometryFlags geomFlags = textures[textureIDs[primitiveID]].isOpaque ? RtGeometryFlags::Opaque : RtGeometryFlags::None;
        bool restartGeomDesc = primitiveID == 0 || geomFlags != geomDesc.flags;
        if (restartGeomDesc)
        {
            if (geomDesc.content.triangles.indexCount > 0)
                geomDescs.push_back(geomDesc);
            geomDesc.content.triangles.indexData += geomDesc.content.triangles.indexCount * sizeof(Index);
            geomDesc.content.triangles.indexCount = 0;
            geomDesc.flags = geomFlags;
        }
        geomDesc.content.triangles.indexCount += 3;
    }

    if (geomDesc.content.triangles.indexCount > 0)
        geomDescs.push_back(geomDesc);

    return geomDescs;
}

} // namespace GSGI