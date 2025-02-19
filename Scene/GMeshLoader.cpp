//
// Created by adamyuan on 11/25/24.
//

#include "GMeshLoader.hpp"

#include "../Util/TextureUtil.hpp"
#include <unordered_map>
#include <Utils/Timing/TimeReport.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace GSGI
{

namespace
{

struct GMeshLoaderContext
{
    using TextureKey = std::variant<std::filesystem::path, uint32_t>;
    struct TextureKeyHash
    {
        std::size_t operator()(const TextureKey& key) const
        {
            return std::visit([]<typename T>(const T& v) -> std::size_t { return std::hash<T>{}(v); }, key);
        }
    };

    ref<Device> pDevice;
    std::filesystem::path basePath;
    GMesh::Data meshData;
    std::unordered_map<TextureKey, GMesh::TextureID, TextureKeyHash> textureIDMap;

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
        uint indexOffset = this->meshData.vertices.size();
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
            this->meshData.vertices.push_back(gVertex);
            this->meshData.bound.include(gVertex.position);
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
                this->meshData.indices.push_back(face.mIndices[j] + indexOffset);
            this->meshData.textureIDs.push_back(textureID);
        }
        return true;
    }
    std::optional<GMesh::TextureID> registerTexture(aiMaterial* pAiMat)
    {
        TextureKey textureKey;
        GMesh::TextureData textureData{};

        // Load texture from file
        if (aiString aiPath; pAiMat->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath) == aiReturn_SUCCESS)
        {
            std::filesystem::path path = basePath / aiPath.C_Str();

            int width, height, channels;
            stbi_uc* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
            if (data)
            {
                // Check opaque or not
                for (int size = width * height, i = 0; i < size; ++i)
                    if (data[i * 4 + 3] < 255)
                    {
                        textureData.isOpaque = false;
                        break;
                    }
                textureData.pTexture = pDevice->createTexture2D(
                    width, height, ResourceFormat::RGBA8Unorm, 1, Resource::kMaxPossible, data, ResourceBindFlags::ShaderResource
                );
                textureData.pTexture->setSourcePath(path);
                textureKey = path;
                stbi_image_free(data);
                logInfo("Loaded {} texture {}", textureData.isOpaque ? "opaque" : "non-opaque", path);
            }
            else
                logWarning("Failed to load texture {}", path);
        }
        // Fallback to color
        if (textureData.pTexture == nullptr)
        {
            std::array<uint8_t, 4> rgba = {255, 255, 255, 255};

            if (aiColor4D aiColor; aiGetMaterialColor(pAiMat, AI_MATKEY_COLOR_DIFFUSE, &aiColor) == aiReturn_SUCCESS)
            {
                rgba[0] = (uint8_t)math::clamp(aiColor.r * 255.0f, 0.0f, 255.0f);
                rgba[1] = (uint8_t)math::clamp(aiColor.g * 255.0f, 0.0f, 255.0f);
                rgba[2] = (uint8_t)math::clamp(aiColor.b * 255.0f, 0.0f, 255.0f);
            }

            textureData.pTexture = createColorTexture(pDevice, rgba.data(), ResourceBindFlags::ShaderResource);
            textureKey = std::bit_cast<uint32_t>(rgba);
        }

        auto it = textureIDMap.find(textureKey);
        if (it != textureIDMap.end())
            return it->second;
        uint uintTextureID = this->meshData.textures.size();
        if (uintTextureID > GMesh::kMaxTextureID)
        {
            logError("Too many textures.");
            return std::nullopt;
        }
        this->meshData.textures.push_back(std::move(textureData));
        GMesh::TextureID textureID = uintTextureID;
        textureIDMap[textureKey] = textureID;
        return textureID;
    }
};

} // namespace

ref<GMesh> GMeshLoader::load(const ref<Device>& pDevice, const std::filesystem::path& filename)
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
            return nullptr;
        }
        pScene = importer.ReadFile(filename.string().c_str(), aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs);
    }
    if (!pScene)
    {
        logError("Failed to open scene: {}", importer.GetErrorString());
        return nullptr;
    }
    timeReport.measure("Loading scene");

    // Create mesh
    GMeshLoaderContext ctx = {
        .pDevice = pDevice,
        .basePath = filename.parent_path(),
        .meshData =
            {
                .path = filename,
                .bound = AABB{},
            },
    };
    if (!ctx.processNode(pScene->mRootNode, pScene) || ctx.meshData.getPrimitiveCount() == 0)
    {
        logError("Failed to create mesh for: {}", importer.GetErrorString());
        return nullptr;
    }
    timeReport.measure("Creating mesh");

    // Normalize mesh through Y direction
    /* {
        float3 center = ctx.meshData.bound.center();
        float3 halfExtent = ctx.meshData.bound.extent() * 0.5f;
        float invHalfExtentY = 1.0f / halfExtent.y;
        const auto normalizeFloat3 = [&](float3& p) { p = (p - center) * invHalfExtentY; };
        for (auto& vertex : ctx.meshData.vertices)
            normalizeFloat3(vertex.position);
        normalizeFloat3(ctx.meshData.bound.minPoint);
        normalizeFloat3(ctx.meshData.bound.maxPoint);
        timeReport.measure("Normalizing mesh");
    } */

    timeReport.printToLog();

    return make_ref<GMesh>(pDevice, std::move(ctx.meshData));
}

} // namespace GSGI