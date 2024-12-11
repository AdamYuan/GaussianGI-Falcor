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

    uint firstOpaquePrimitiveID = -1; // -1 means not computed

    uint getIndexCount() const { return indices.size(); }
    uint getPrimitiveCount() const { return indices.size() / 3; }
    uint getVertexCount() const { return vertices.size(); }
    uint getTextureCount() const { return textures.size(); }
    bool hasOpaquePrimitive() const { return firstOpaquePrimitiveID < getPrimitiveCount(); }
    uint getOpaquePrimitiveCount() const { return getPrimitiveCount() - firstOpaquePrimitiveID; }
    bool hasNonOpaquePrimitive() const { return firstOpaquePrimitiveID > 0; }
    uint getNonOpaquePrimitiveCount() const { return firstOpaquePrimitiveID; }

    void reorderOpaque();

    static ref<VertexLayout> createVertexLayout();
    static ResourceFormat getIndexFormat() { return ResourceFormat::R32Uint; }
    template<RtGeometryFlags Flags>
    RtGeometryDesc getRTGeometryDesc(DeviceAddress transform3x4Addr, DeviceAddress indexBufferAddr, DeviceAddress vertexBufferAddr) const
    {
        static_assert(Flags == RtGeometryFlags::Opaque || Flags == RtGeometryFlags::None);
        return RtGeometryDesc{
            .type = RtGeometryType::Triangles,
            .flags = Flags,
            .content =
                {.triangles =
                     {
                         .transform3x4 = transform3x4Addr,
                         .indexFormat = ResourceFormat::R32Uint,
                         .vertexFormat = ResourceFormat::RGB32Float,
                         .indexCount = 3 * (Flags == RtGeometryFlags::Opaque ? getOpaquePrimitiveCount() : getNonOpaquePrimitiveCount()),
                         .vertexCount = 0, // Seems to work (at least on Vulkan)
                         .indexData = indexBufferAddr + sizeof(Index) * 3 * (Flags == RtGeometryFlags::Opaque ? firstOpaquePrimitiveID : 0),
                         .vertexData = vertexBufferAddr,
                         .vertexStride = sizeof(Vertex),
                     }}
        };
    }

    static Ptr createPtr(GMesh&& mesh)
    {
        if (mesh.firstOpaquePrimitiveID == -1)
            mesh.reorderOpaque();
        return std::make_shared<const GMesh>(std::move(mesh));
    }
};

} // namespace GSGI

#endif
