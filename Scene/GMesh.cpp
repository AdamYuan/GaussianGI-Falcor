//
// Created by adamyuan on 12/11/24.
//
#include "GMesh.hpp"

#include "GMesh.slangh"

namespace GSGI
{

static_assert(GMesh::kMaxTextureCount == GMESH_MAX_TEXTURE_COUNT);

void GMesh::reorderOpaque()
{
    uint primitiveCount = this->getPrimitiveCount();
    std::vector<std::pair<bool, uint>> opaqueOrders(primitiveCount);
    for (uint primitiveID = 0; primitiveID < primitiveCount; ++primitiveID)
        opaqueOrders[primitiveID] = {this->textures[this->textureIDs[primitiveID]].isOpaque, primitiveID};
    std::stable_sort(opaqueOrders.begin(), opaqueOrders.end(), [](const auto& l, const auto& r) { return l.first < r.first; });

    std::vector<Index> indices(primitiveCount * 3);
    std::vector<TextureID> textureIDs(primitiveCount);

    this->firstOpaquePrimitiveID = -1;
    for (uint dstPrimitiveID = 0; dstPrimitiveID < primitiveCount; ++dstPrimitiveID)
    {
        if (opaqueOrders[dstPrimitiveID].first && this->firstOpaquePrimitiveID == -1) // is opaque and not computed
            this->firstOpaquePrimitiveID = dstPrimitiveID;
        uint srcPrimitiveID = opaqueOrders[dstPrimitiveID].second;
        indices[dstPrimitiveID * 3 + 0] = this->indices[srcPrimitiveID * 3 + 0];
        indices[dstPrimitiveID * 3 + 1] = this->indices[srcPrimitiveID * 3 + 1];
        indices[dstPrimitiveID * 3 + 2] = this->indices[srcPrimitiveID * 3 + 2];
        textureIDs[dstPrimitiveID] = this->textureIDs[srcPrimitiveID];
    }
    if (this->firstOpaquePrimitiveID == -1) // still not computed
        this->firstOpaquePrimitiveID = primitiveCount;
    this->indices = std::move(indices);
    this->textureIDs = std::move(textureIDs);

    logInfo("Non-Opaque: [{}, {}), Opaque: [{}, {})", 0, this->firstOpaquePrimitiveID, this->firstOpaquePrimitiveID, primitiveCount);
}

void GMesh::updateBound()
{
    this->bound = {};
    for (const auto& vertex : this->vertices)
        this->bound.include(vertex.position);
}

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

} // namespace GSGI