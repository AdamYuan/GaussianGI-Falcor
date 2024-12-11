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

std::optional<RtGeometryDesc> GMesh::getRTGeometryDesc(
    RtGeometryFlags flag,
    DeviceAddress transform3x4Addr,
    DeviceAddress indexBufferAddr,
    DeviceAddress vertexBufferAddr
) const
{
    // TODO: Support non-opaque triangles
    if (flag != RtGeometryFlags::Opaque)
        return std::nullopt;

    RtGeometryDesc geomDesc{};
    geomDesc.type = RtGeometryType::Triangles;
    geomDesc.flags = flag;
    geomDesc.content.triangles = {
        .transform3x4 = transform3x4Addr,
        .indexFormat = ResourceFormat::R32Uint,
        .vertexFormat = ResourceFormat::RGB32Float,
        .indexCount = getIndexCount(),
        .vertexCount = 0, // Seems to work (at least on Vulkan)
        .indexData = indexBufferAddr,
        .vertexData = vertexBufferAddr,
        .vertexStride = sizeof(Vertex),
    };

    return geomDesc;
}

} // namespace GSGI