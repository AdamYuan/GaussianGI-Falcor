//
// Created by adamyuan on 11/25/24.
//

#include "GMeshLoader.hpp"

#include <unordered_map>
#include <Utils/Timing/TimeReport.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace GSGI
{

namespace
{

struct GMeshLoaderContext
{
    std::filesystem::path basePath;
    GMesh mesh;
    std::unordered_map<std::filesystem::path, GMesh::TextureID> textureIDMap;

    bool processNode(const aiNode* pAiNode, const aiScene* pAiScene)
    {
        // skip empty nodes
        if (!pAiNode)
            return true;
        // process all the node's meshes (if any)
        for (uint i = 0; i < pAiNode->mNumMeshes; i++)
        {
            aiMesh* mesh = pAiScene->mMeshes[pAiNode->mMeshes[i]];
            if (!processMesh(mesh, pAiScene))
                return false;
        }
        // then do the same for each of its children
        for (uint i = 0; i < pAiNode->mNumChildren; i++)
        {
            if (!processNode(pAiNode->mChildren[i], pAiScene))
                return false;
        }
        return true;
    }
    bool processMesh(const aiMesh* pAiMesh, const aiScene* pAiScene)
    {
        uint indexOffset = this->mesh.vertices.size();
        // process vertex positions, normals and texture coordinates
        for (uint i = 0; i < pAiMesh->mNumVertices; i++)
        {
            const auto& aiVertex = pAiMesh->mVertices[i];
            const auto& aiNormal = pAiMesh->mNormals[i];
            GMesh::Vertex gVertex = {
                .position = {aiVertex.x, aiVertex.y, aiVertex.z},
                .normal = {aiNormal.x, aiNormal.y, aiNormal.z},
            };
            if (pAiMesh->mTextureCoords[0])
            {
                const auto& aiTexcoord = pAiMesh->mTextureCoords[0][i];
                gVertex.texcoord = {aiTexcoord.x, aiTexcoord.y};
            }
            this->mesh.vertices.push_back(gVertex);
            this->mesh.bound.merge(gVertex.position);
        }
        // process texture
        if (pAiMesh->mMaterialIndex >= pAiScene->mNumMaterials)
        {
            logError("No material found.");
            return false;
        }
        auto optTextureID = registerTexture(pAiScene->mMaterials[pAiMesh->mMaterialIndex]);
        if (!optTextureID)
            return false;
        auto textureID = *optTextureID;
        // process indices
        for (uint i = 0; i < pAiMesh->mNumFaces; i++)
        {
            aiFace face = pAiMesh->mFaces[i];
            for (uint j = 0; j < face.mNumIndices; j++)
                this->mesh.indices.push_back(face.mIndices[j] + indexOffset);
            this->mesh.textureIDs.push_back(textureID);
        }
        return true;
    }
    std::optional<GMesh::TextureID> registerTexture(aiMaterial* pAiMat)
    {
        aiString aiPath;
        if (pAiMat->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath) != aiReturn_SUCCESS)
        {
            logError("Missing texture.");
            return std::nullopt;
        }
        std::filesystem::path path = aiPath.C_Str();

        auto it = textureIDMap.find(path);
        if (it != textureIDMap.end())
            return it->second;
        uint uintTextureID = this->mesh.texturePaths.size();
        if (uintTextureID > GMesh::kMaxTextureID)
        {
            logError("Too many textures.");
            return std::nullopt;
        }
        this->mesh.texturePaths.push_back(basePath / path);
        GMesh::TextureID textureID = uintTextureID;
        textureIDMap[path] = textureID;
        return textureID;
    }
};

} // namespace

std::optional<GMesh> GMeshLoader::load(const std::filesystem::path& filename)
{
    TimeReport timeReport;

    Assimp::Importer importer;

    // Load scene
    const aiScene* pScene = nullptr;
    if (!filename.empty())
    {
        if (!filename.is_absolute())
        {
            logError("Expected absolute path.");
            return std::nullopt;
        }
        pScene = importer.ReadFile(filename.string().c_str(), aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs);
    }
    if (!pScene)
    {
        logError("Failed to open scene: {}", importer.GetErrorString());
        return std::nullopt;
    }
    timeReport.measure("Loading scene");

    // Create mesh
    GMeshLoaderContext ctx = {
        .basePath = filename.parent_path(),
        .mesh =
            {
                .path = filename,
                .bound = GBound{},
            },
    };
    if (!ctx.processNode(pScene->mRootNode, pScene) || ctx.mesh.isEmpty())
    {
        logError("Failed to create mesh for: {}", importer.GetErrorString());
        return std::nullopt;
    }
    timeReport.measure("Creating mesh");

    // Normalize mesh through Y direction
    float3 center = ctx.mesh.bound.getCenter();
    float3 halfExtent = ctx.mesh.bound.getExtent() * 0.5f;
    float invHalfExtentY = 1.0f / halfExtent.y;
    const auto normalizeFloat3 = [&](float3& p) { p = (p - center) * invHalfExtentY; };
    for (auto& vertex : ctx.mesh.vertices)
        normalizeFloat3(vertex.position);
    normalizeFloat3(ctx.mesh.bound.bMin);
    normalizeFloat3(ctx.mesh.bound.bMax);
    timeReport.measure("Normalizing mesh");

    timeReport.printToLog();

    return ctx.mesh;
}

} // namespace GSGI