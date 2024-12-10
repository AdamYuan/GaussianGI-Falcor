//
// Created by adamyuan on 11/25/24.
//

#pragma once
#ifndef GSGI_GMESH_HPP
#define GSGI_GMESH_HPP

#include <Falcor.h>

#include "GBound.hpp"
#include "GMesh.slangh"

using namespace Falcor;

namespace GSGI
{

struct GMesh
{
    using Ptr = std::shared_ptr<const GMesh>;

    struct Vertex
    {
        float3 position;
        float3 normal;
        float2 texcoord;
    };

    using TextureID = uint8_t;
    static constexpr TextureID kMaxTextureID = std::numeric_limits<TextureID>::max();
    static_assert(kMaxTextureID == GMESH_MAX_TEXTURE_COUNT - 1);
    using Index = uint32_t;

    std::filesystem::path path;
    GBound bound;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;
    std::vector<TextureID> textureIDs; // per-triangle
    std::vector<std::filesystem::path> texturePaths;

    uint getIndexCount() const { return indices.size(); }
    uint getPrimitiveCount() const { return indices.size() / 3; }
    uint getVertexCount() const { return vertices.size(); }
    uint getTextureCount() const { return texturePaths.size(); }

    static ref<VertexLayout> createVertexLayout()
    {
        auto vertexBufferLayout = VertexBufferLayout::create();
        vertexBufferLayout->addElement(GMESH_VERTEX_POSITION_NAME, 0, ResourceFormat::RGB32Float, 1, GMESH_VERTEX_POSITION_LOC);
        vertexBufferLayout->addElement(GMESH_VERTEX_NORMAL_NAME, 3 * sizeof(float), ResourceFormat::RGB32Float, 1, GMESH_VERTEX_NORMAL_LOC);
        vertexBufferLayout->addElement(
            GMESH_VERTEX_TEXCOORD_NAME, 6 * sizeof(float), ResourceFormat::RG32Float, 1, GMESH_VERTEX_TEXCOORD_LOC
        );
        auto vertexLayout = VertexLayout::create();
        vertexLayout->addBufferLayout(0, std::move(vertexBufferLayout));
        return vertexLayout;
    }
    static ResourceFormat getIndexFormat() { return ResourceFormat::R32Uint; }
    std::optional<RtGeometryDesc> getRTGeometryDesc(
        RtGeometryFlags flag,
        DeviceAddress transform3x4Addr,
        DeviceAddress indexBufferAddr,
        DeviceAddress vertexBufferAddr,
        uint vertexCount
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
            .vertexCount = vertexCount,
            .indexData = indexBufferAddr,
            .vertexData = vertexBufferAddr,
            .vertexStride = sizeof(Vertex),
        };

        return geomDesc;
    }
};

} // namespace GSGI

#endif
