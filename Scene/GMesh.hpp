//
// Created by adamyuan on 11/25/24.
//

#pragma once
#ifndef GSGI_GMESH_HPP
#define GSGI_GMESH_HPP

#include <Falcor.h>

#include "GBound.hpp"

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
    static constexpr uint32_t kMaxTextureCount = uint32_t(kMaxTextureID) + 1;
    using Index = uint32_t;

    struct TextureInfo
    {
        ref<Texture> pTexture;
        bool isOpaque = true;
    };

    std::filesystem::path path;
    GBound bound;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;
    std::vector<TextureID> textureIDs; // per-triangle
    std::vector<TextureInfo> textures;

    uint getIndexCount() const { return indices.size(); }
    uint getPrimitiveCount() const { return indices.size() / 3; }
    uint getVertexCount() const { return vertices.size(); }
    uint getTextureCount() const { return textures.size(); }

    static ref<VertexLayout> createVertexLayout();
    static ResourceFormat getIndexFormat() { return ResourceFormat::R32Uint; }
    std::vector<RtGeometryDesc> getRTGeometryDescs(
        DeviceAddress transform3x4Addr,
        DeviceAddress indexBufferAddr,
        DeviceAddress vertexBufferAddr
    ) const;
};

} // namespace GSGI

#endif
