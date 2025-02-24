//
// Created by adamyuan on 12/1/24.
//

#include "GStaticScene.hpp"

#include <ranges>

namespace GSGI
{

void GStaticScene::import(const ref<GScene>& pScene)
{
    mpMeshes = getSceneMeshes(pScene);

    std::vector<GMesh::Vertex> vertices;
    std::vector<GMesh::Index> indices;
    std::vector<GMesh::TextureID> textureIDs;
    std::vector<gfx::IndirectDrawIndexedArguments> drawCommands;

    mpTextures.clear();
    drawCommands.reserve(mpMeshes.size());
    mMeshInfos.clear();
    mMeshInfos.reserve(mpMeshes.size());
    mInstanceInfos.clear();
    mInstanceInfos.reserve(pScene->getInstanceCount());
    for (std::size_t meshID = 0; meshID < mpMeshes.size(); ++meshID)
    {
        const auto& entry = pScene->getMeshEntries()[meshID];
        const auto& pMesh = mpMeshes[meshID];

        MeshInfo meshInfo = {
            .indexCount = pMesh->getIndexCount(),
            .instanceCount = (uint)entry.instances.size(),
            .firstIndex = (uint)indices.size(),
            .firstInstance = (uint)mInstanceInfos.size(),
        };

        gfx::IndirectDrawIndexedArguments drawCmd = {
            .IndexCountPerInstance = (gfx::GfxCount)meshInfo.indexCount,
            .InstanceCount = (gfx::GfxCount)meshInfo.instanceCount,
            .StartIndexLocation = (gfx::GfxIndex)meshInfo.firstIndex,
            .BaseVertexLocation = 0,
            .StartInstanceLocation = (gfx::GfxIndex)meshInfo.firstInstance,
        };

        uint baseIndex = vertices.size(), baseTextureID = mpTextures.size();

        // Mesh buffer
        drawCommands.push_back(drawCmd);
        mMeshInfos.push_back(meshInfo);
        // Vertex buffer
        vertices.insert(vertices.end(), pMesh->getData().vertices.begin(), pMesh->getData().vertices.end());
        // Index buffer
        auto paddedIndices =
            pMesh->getData().indices | std::views::transform([&](GMesh::Index x) -> GMesh::Index { return x + baseIndex; });
        indices.insert(indices.end(), paddedIndices.begin(), paddedIndices.end());
        // Texture ID buffer
        auto paddedTextureIDs =
            pMesh->getData().textureIDs | std::views::transform([&](GMesh::TextureID x) -> GMesh::TextureID { return x + baseTextureID; });
        textureIDs.insert(textureIDs.end(), paddedTextureIDs.begin(), paddedTextureIDs.end());
        // Textures
        auto textures =
            pMesh->getData().textures | std::views::transform([&](const GMesh::TextureData& x) -> ref<Texture> { return x.pTexture; });
        mpTextures.insert(mpTextures.end(), textures.begin(), textures.end());
        // Instance buffer
        for (const auto& instance : entry.instances)
        {
            InstanceInfo instanceInfo = {
                .transform = instance.transform,
                .meshID = (uint)meshID,
            };
            mInstanceInfos.push_back(instanceInfo);
        }
    }

    FALCOR_CHECK(mInstanceInfos.size() <= kMaxInstanceCount, "");

    mpVertexBuffer = getDevice()->createStructuredBuffer(
        sizeof(GMesh::Vertex), //
        vertices.size(),
        ResourceBindFlags::Vertex | ResourceBindFlags::ShaderResource,
        MemoryType::DeviceLocal,
        vertices.data()
    );

    static_assert(std::same_as<GMesh::Index, uint32_t>);
    mpIndexBuffer = getDevice()->createStructuredBuffer(
        sizeof(GMesh::Index), //
        indices.size(),
        ResourceBindFlags::Index | ResourceBindFlags::ShaderResource,
        MemoryType::DeviceLocal,
        indices.data()
    );

    mpVao = Vao::create(Vao::Topology::TriangleList, GMesh::getVertexLayout(), {mpVertexBuffer}, mpIndexBuffer, GMesh::getIndexFormat());

    static_assert(std::same_as<GMesh::TextureID, uint8_t>);
    mpTextureIDBuffer = getDevice()->createTypedBuffer(
        ResourceFormat::R8Uint, textureIDs.size(), ResourceBindFlags::ShaderResource, MemoryType::DeviceLocal, textureIDs.data()
    );

    mpDrawCmdBuffer = getDevice()->createStructuredBuffer(
        sizeof(gfx::IndirectDrawIndexedArguments), //
        drawCommands.size(),
        ResourceBindFlags::IndirectArg,
        MemoryType::DeviceLocal,
        drawCommands.data()
    );

    mpMeshInfoBuffer = getDevice()->createStructuredBuffer(
        sizeof(MeshInfo), //
        mMeshInfos.size(),
        ResourceBindFlags::ShaderResource,
        MemoryType::DeviceLocal,
        mMeshInfos.data()
    );

    mpInstanceInfoBuffer = getDevice()->createStructuredBuffer(
        sizeof(InstanceInfo), //
        mInstanceInfos.size(),
        ResourceBindFlags::ShaderResource,
        MemoryType::DeviceLocal,
        mInstanceInfos.data()
    );

    if (mpTextures.size() > GMesh::kMaxTextureCount)
    {
        logWarning("Too many textures in GStaticScene");
        mpTextures.resize(GMesh::kMaxTextureCount);
    }
}

std::vector<ref<GMesh>> GStaticScene::getSceneMeshes(const ref<GScene>& pScene)
{
    std::vector<ref<GMesh>> meshes;
    meshes.reserve(pScene->getMeshEntries().size());
    for (const auto& entry : pScene->getMeshEntries())
        meshes.push_back(entry.pMesh);
    return meshes;
}

void GStaticScene::bindRootShaderData(const ShaderVar& rootVar) const
{
    auto var = rootVar["gGStaticScene"];
    var["rtAccel"].setAccelerationStructure(mpTLAS);
    var["vertices"] = mpVertexBuffer;
    var["indices"] = mpIndexBuffer;
    var["textureIDs"] = mpTextureIDBuffer;
    var["meshInfos"] = mpMeshInfoBuffer;
    var["instanceInfos"] = mpInstanceInfoBuffer;
    var["sampler"] = getDevice()->getDefaultSampler();
    var["meshCount"] = (uint)getMeshCount();
    var["instanceCount"] = (uint)getInstanceCount();
    mpCamera->bindShaderData(var["camera"]);
    mpLighting->bindShaderData(var["lighting"]);
    for (int textureID = 0; textureID < (int)mpTextures.size(); ++textureID)
        var["textures"][textureID] = mpTextures[textureID];
}

void GStaticScene::draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const
{
    bindRootShaderData(pRasterPass->getRootVar());
    pRasterPass->getState()->setFbo(pFbo);
    pRasterPass->getState()->setVao(mpVao);
    pRenderContext->drawIndexedIndirect(
        pRasterPass->getState().get(), pRasterPass->getVars().get(), mpMeshes.size(), mpDrawCmdBuffer.get(), 0, nullptr, 0
    );
}

} // namespace GSGI