//
// Created by adamyuan on 11/25/24.
//

#pragma once
#ifndef GSGI_GMESH_HPP
#define GSGI_GMESH_HPP

#include "../GDeviceObject.hpp"
#include <Falcor.h>
#include <Core/Pass/RasterPass.h>
#include <Utils/Math/AABB.h>

using namespace Falcor;

namespace GSGI
{

class GMesh final : public GDeviceObject<GMesh>
{
public:
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

    static constexpr float3 kNormalizedBoundMin = float3{-1.0f}, kNormalizedBoundMax = float3{1.0f};
    inline static const AABB kNormalizeBound = AABB(kNormalizedBoundMin, kNormalizedBoundMax);

    struct TextureData
    {
        ref<Texture> pTexture;
        bool isOpaque;
    };

    struct Data
    {
        std::filesystem::path path;
        AABB bound;
        std::vector<Vertex> vertices;
        std::vector<Index> indices;
        std::vector<TextureID> textureIDs; // per-triangle
        std::vector<TextureData> textures;
        uint firstOpaquePrimitiveID = -1; // -1 means not computed

        uint getIndexCount() const { return indices.size(); }
        uint getPrimitiveCount() const { return indices.size() / 3; }
        uint getVertexCount() const { return vertices.size(); }
        uint getTextureCount() const { return textures.size(); }
        bool hasOpaquePrimitive() const { return firstOpaquePrimitiveID < getPrimitiveCount(); }
        uint getOpaquePrimitiveCount() const { return getPrimitiveCount() - firstOpaquePrimitiveID; }
        bool hasNonOpaquePrimitive() const { return firstOpaquePrimitiveID > 0; }
        uint getNonOpaquePrimitiveCount() const { return firstOpaquePrimitiveID; }
    };

private:
    Data mData;
    ref<Vao> mpVao;
    ref<Buffer> mpVertexBuffer, mpIndexBuffer, mpTextureIDBuffer;
    static ref<VertexLayout> spVertexLayout;
    static ref<RasterizerState> spRasterizerState;

    static void dataReorderOpaque(Data& data);
    static void dataUpdateBound(Data& data);
    static void dataNormalizeBound(Data& data);
    void prepareDraw();
    void prepareBuffer();

public:
    GMesh(ref<Device> pDevice, Data data);

    const auto& getData() const { return mData; }
    const std::filesystem::path& getSourcePath() const { return mData.path; }
    std::filesystem::path getPersistPath(std::string_view keyStr) const;

#define GMESH_DATA_FUNC(X) \
    auto X() const { return mData.X(); }
    GMESH_DATA_FUNC(getIndexCount)
    GMESH_DATA_FUNC(getPrimitiveCount)
    GMESH_DATA_FUNC(getVertexCount)
    GMESH_DATA_FUNC(getTextureCount)
    GMESH_DATA_FUNC(hasOpaquePrimitive)
    GMESH_DATA_FUNC(getOpaquePrimitiveCount)
    GMESH_DATA_FUNC(hasNonOpaquePrimitive)
    GMESH_DATA_FUNC(getNonOpaquePrimitiveCount)
#undef GMESH_DATA_FUNC
    const AABB& getBound() const { return mData.bound; }

    static ref<VertexLayout> getVertexLayout() { return spVertexLayout; }
    static ref<RasterizerState> getRasterizerState() { return spRasterizerState; }
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
                         .indexData =
                             indexBufferAddr + sizeof(Index) * 3 * (Flags == RtGeometryFlags::Opaque ? mData.firstOpaquePrimitiveID : 0),
                         .vertexData = vertexBufferAddr,
                         .vertexStride = sizeof(Vertex),
                     }}
        };
    }

    void renderUIImpl(Gui::Widgets& widget) const;
    void draw(RenderContext* pRenderContext, const ref<RasterPass>& pRasterPass, const ShaderVar& rasterDataVar);

    void bindShaderData(const ShaderVar& var);
};

} // namespace GSGI

#endif
