//
// Created by adamyuan on 11/25/24.
//

#pragma once
#ifndef GSGI_GMESH_HPP
#define GSGI_GMESH_HPP

#include <Falcor.h>

#include <unordered_map>
#include <unordered_set>
#include "GBound.hpp"

using namespace Falcor;

namespace GSGI
{

struct GMesh
{
    struct Vertex
    {
        float3 position;
        float3 normal;
        float2 texcoord;
    };

    using TextureID = uint8_t;
    static constexpr TextureID kMaxTextureID = std::numeric_limits<TextureID>::max();
    using Index = uint32_t;

    std::filesystem::path path;
    GBound bound;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;
    std::vector<TextureID> textureIDs; // per-triangle
    std::vector<std::filesystem::path> texturePaths;

    bool isEmpty() const { return indices.empty(); }
    bool isLoaded() const { return !isEmpty(); }
    uint getIndexCount() const { return indices.size(); }
    uint getPrimitiveCount() const { return indices.size() / 3; }
    uint getVertexCount() const { return vertices.size(); }
    uint getTextureCount() const { return texturePaths.size(); }

    static ref<VertexLayout> createVertexLayout()
    {
        auto vertexBufferLayout = VertexBufferLayout::create();
        vertexBufferLayout->addElement("position", 0, ResourceFormat::RGB32Float, 1, 0);
        vertexBufferLayout->addElement("normal", 3 * sizeof(float), ResourceFormat::RGB32Float, 1, 1);
        vertexBufferLayout->addElement("texcoord", 6 * sizeof(float), ResourceFormat::RG32Float, 1, 2);
        auto vertexLayout = VertexLayout::create();
        vertexLayout->addBufferLayout(0, std::move(vertexBufferLayout));
        return vertexLayout;
    }
};

} // namespace GSGI

#endif
