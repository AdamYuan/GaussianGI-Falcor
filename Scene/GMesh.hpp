//
// Created by adamyuan on 11/25/24.
//

#pragma once
#ifndef GSGI_GMESH_HPP
#define GSGI_GMESH_HPP

#include <Falcor.h>

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

    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<TextureID> textureIDs; // per-triangle
    std::vector<std::filesystem::path> texturePaths;

    bool empty() const { return indices.empty(); }
};

} // namespace GSGI

#endif
