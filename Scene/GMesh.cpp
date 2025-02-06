//
// Created by adamyuan on 12/11/24.
//
#include "GMesh.hpp"

#include "GMesh.slangh"

namespace GSGI
{

static_assert(GMesh::kMaxTextureCount == GMESH_MAX_TEXTURE_COUNT);

ref<VertexLayout> GMesh::spVertexLayout = []
{
    auto vertexBufferLayout = VertexBufferLayout::create();
    vertexBufferLayout->addElement(GMESH_VERTEX_POSITION_NAME, 0, ResourceFormat::RGB32Float, 1, GMESH_VERTEX_POSITION_LOC);
    vertexBufferLayout->addElement(GMESH_VERTEX_NORMAL_NAME, 3 * sizeof(float), ResourceFormat::RGB32Float, 1, GMESH_VERTEX_NORMAL_LOC);
    vertexBufferLayout->addElement(GMESH_VERTEX_TEXCOORD_NAME, 6 * sizeof(float), ResourceFormat::RG32Float, 1, GMESH_VERTEX_TEXCOORD_LOC);
    auto pVertexLayout = VertexLayout::create();
    pVertexLayout->addBufferLayout(0, std::move(vertexBufferLayout));
    return pVertexLayout;
}();

ref<RasterizerState> GMesh::spRasterizerState = []
{
    RasterizerState::Desc desc = {};
    desc.setCullMode(RasterizerState::CullMode::None);
    return RasterizerState::create(desc);
}();

GMesh::GMesh(ref<Device> pDevice, Data data) : GDeviceObject(std::move(pDevice))
{
    if (data.firstOpaquePrimitiveID == -1)
        dataReorderOpaque(data);
    if (!data.bound.valid())
        dataUpdateBound(data);
    mData = std::move(data);
}

void GMesh::dataReorderOpaque(Data& data)
{
    uint primitiveCount = data.getPrimitiveCount();
    std::vector<std::pair<bool, uint>> opaqueOrders(primitiveCount);
    for (uint primitiveID = 0; primitiveID < primitiveCount; ++primitiveID)
        opaqueOrders[primitiveID] = {data.textures[data.textureIDs[primitiveID]].isOpaque, primitiveID};
    std::stable_sort(opaqueOrders.begin(), opaqueOrders.end(), [](const auto& l, const auto& r) { return l.first < r.first; });

    std::vector<Index> indices(primitiveCount * 3);
    std::vector<TextureID> textureIDs(primitiveCount);

    data.firstOpaquePrimitiveID = -1;
    for (uint dstPrimitiveID = 0; dstPrimitiveID < primitiveCount; ++dstPrimitiveID)
    {
        if (opaqueOrders[dstPrimitiveID].first && data.firstOpaquePrimitiveID == -1) // is opaque and not computed
            data.firstOpaquePrimitiveID = dstPrimitiveID;
        uint srcPrimitiveID = opaqueOrders[dstPrimitiveID].second;
        indices[dstPrimitiveID * 3 + 0] = data.indices[srcPrimitiveID * 3 + 0];
        indices[dstPrimitiveID * 3 + 1] = data.indices[srcPrimitiveID * 3 + 1];
        indices[dstPrimitiveID * 3 + 2] = data.indices[srcPrimitiveID * 3 + 2];
        textureIDs[dstPrimitiveID] = data.textureIDs[srcPrimitiveID];
    }
    if (data.firstOpaquePrimitiveID == -1) // still not computed
        data.firstOpaquePrimitiveID = primitiveCount;
    data.indices = std::move(indices);
    data.textureIDs = std::move(textureIDs);

    logInfo("Non-Opaque: [{}, {}), Opaque: [{}, {})", 0, data.firstOpaquePrimitiveID, data.firstOpaquePrimitiveID, primitiveCount);
}

void GMesh::dataUpdateBound(Data& data)
{
    data.bound = {};
    for (const auto& vertex : data.vertices)
        data.bound.include(vertex.position);
}

void GMesh::prepareBuffer()
{
    if (mpIndexBuffer == nullptr)
    {
        static_assert(std::same_as<Index, uint32_t>);
        mpIndexBuffer = getDevice()->createStructuredBuffer(
            sizeof(Index), //
            getIndexCount(),
            ResourceBindFlags::Index | ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            mData.indices.data()
        );
    }
    if (mpVertexBuffer == nullptr)
    {
        mpVertexBuffer = getDevice()->createStructuredBuffer(
            sizeof(Vertex), //
            getVertexCount(),
            ResourceBindFlags::Vertex | ResourceBindFlags::ShaderResource,
            MemoryType::DeviceLocal,
            mData.vertices.data()
        );
    }
    if (mpTextureIDBuffer == nullptr)
    {
        static_assert(std::same_as<TextureID, uint8_t>);
        mpTextureIDBuffer = getDevice()->createTypedBuffer(
            ResourceFormat::R8Uint, getPrimitiveCount(), ResourceBindFlags::ShaderResource, MemoryType::DeviceLocal, mData.textureIDs.data()
        );
    }
}

void GMesh::prepareDraw()
{
    prepareBuffer();
    if (mpVao == nullptr)
        mpVao = Vao::create(Vao::Topology::TriangleList, getVertexLayout(), {mpVertexBuffer}, mpIndexBuffer, getIndexFormat());
}

void GMesh::renderUIImpl(Gui::Widgets& widget) const
{
    widget.text(fmt::format("Path: {}", getSourcePath().string()));
    widget.text(fmt::format("Vertex Count: {}", getVertexCount()));
    widget.text(fmt::format("Primitive Count: {}", getPrimitiveCount()));
    widget.text(fmt::format("Texture Count: {}", getTextureCount()));
    widget.text(fmt::format("AABB: ({}, {})", getBound().minPoint, getBound().maxPoint));
}

void GMesh::draw(RenderContext* pRenderContext, const ref<RasterPass>& pRasterPass, const ShaderVar& rasterDataVar)
{
    prepareDraw();
    rasterDataVar["textureIDs"] = mpTextureIDBuffer;
    for (uint32_t textureID = 0; textureID < getTextureCount(); ++textureID)
        rasterDataVar["textures"][textureID] = mData.textures[textureID].pTexture;
    pRasterPass->getState()->setVao(mpVao);
    pRenderContext->drawIndexed(pRasterPass->getState().get(), pRasterPass->getVars().get(), getIndexCount(), 0, 0);
}

void GMesh::bindShaderData(const ShaderVar& var)
{
    prepareBuffer();
    var["vertices"] = mpVertexBuffer;
    var["indices"] = mpIndexBuffer;
    var["textureIDs"] = mpTextureIDBuffer;
    for (uint32_t textureID = 0; textureID < getTextureCount(); ++textureID)
        var["textures"][textureID] = mData.textures[textureID].pTexture;
}

std::filesystem::path GMesh::getPersistPath(std::string_view keyStr) const
{
    std::string filename = mData.path.filename().string();
    std::replace(filename.begin(), filename.end(), '.', '_');
    return getSourcePath().parent_path() / fmt::format("{}_GSGI_GMesh_{}.bin", filename, keyStr);
}

} // namespace GSGI