//
// Created by adamyuan on 11/25/24.
//

#include "GScene.hpp"

#include "GMeshLoader.hpp"
#include <unordered_set>
#include <unordered_map>

namespace GSGI
{

GScene::GScene(ref<Device> pDevice) : GDeviceObject(std::move(pDevice))
{
    mpDefaultRasterPass = RasterPass::create(getDevice(), "GaussianGI/Scene/GScene.3d.slang", "vsMain", "psMain");
    mpSampler = getDevice()->getDefaultSampler();
    RasterizerState::Desc rasterStateDesc = {};
    rasterStateDesc.setCullMode(RasterizerState::CullMode::None);
    mpRasterState = RasterizerState::create(rasterStateDesc);
}

static void removeVectorIndices(auto& vector, auto& indexSet)
{
    if (!indexSet.empty())
        vector.erase(
            std::remove_if(vector.begin(), vector.end(), [&](const auto& e) { return indexSet.contains(&e - vector.data()); }), vector.end()
        );
}

void GScene::update_makeUnique()
{
    std::unordered_map<std::filesystem::path, std::size_t> meshPathIndexMap;
    std::unordered_set<std::size_t> removeIndexSet;
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        auto& entry = mEntries[i];
        auto mapIt = meshPathIndexMap.find(entry.mesh.path);
        if (mapIt == meshPathIndexMap.end())
        {
            meshPathIndexMap[entry.mesh.path] = i;
            continue;
        }
        auto& uniqueEntry = mEntries[mapIt->second];
        // Move instances to unique entry
        uniqueEntry.instances.insert(
            uniqueEntry.instances.end(), //
            std::make_move_iterator(entry.instances.begin()),
            std::make_move_iterator(entry.instances.end())
        );
        removeIndexSet.insert(i);
    }

    removeVectorIndices(mEntries, removeIndexSet);
}

void GScene::update_loadMesh()
{
    std::unordered_set<std::size_t> removeIndexSet;
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        auto& entry = mEntries[i];
        if (entry.mesh.isLoaded())
            continue;

        std::optional<GMesh> optMesh = GMeshLoader::load(entry.mesh.path);
        if (!optMesh)
        {
            logWarning("Failed to load mesh {}", entry.mesh.path);
            removeIndexSet.insert(i);
        }
        else
        {
            logInfo("Loaded mesh {}", entry.mesh.path);
            entry.mesh = std::move(*optMesh);
        }
    }

    removeVectorIndices(mEntries, removeIndexSet);
}

void GScene::update_loadTexture()
{
    for (auto& entry : mEntries)
    {
        if (!entry.mesh.isLoaded())
            continue;

        if (entry.pTextures.empty())
        {
            entry.pTextures.reserve(entry.mesh.texturePaths.size());
            for (const auto& texPath : entry.mesh.texturePaths)
            {
                auto tex = Texture::createFromFile(getDevice(), texPath, true, false);
                if (tex == nullptr)
                {
                    logWarning("Failed to load texture {}", texPath);
                    if (mpDefaultTexture == nullptr)
                    {
                        uint8_t color[] = {0xFF, 0x00, 0xFF, 0xFF};
                        mpDefaultTexture = getDevice()->createTexture2D(1, 1, ResourceFormat::RGBA8Uint, 1, Resource::kMaxPossible, color);
                    }
                    tex = mpDefaultTexture;
                }
                else
                    logInfo("Loaded texture {}", texPath);
                entry.pTextures.push_back(std::move(tex));
            }
        }
    }
}

void GScene::update_createBuffer()
{
    for (auto& entry : mEntries)
    {
        if (!entry.mesh.isLoaded())
            continue;

        if (entry.pIndexBuffer == nullptr)
        {
            static_assert(std::same_as<GMesh::Index, uint32_t>);
            entry.pIndexBuffer = getDevice()->createStructuredBuffer(
                sizeof(GMesh::Index), //
                entry.mesh.getIndexCount(),
                ResourceBindFlags::Index,
                MemoryType::DeviceLocal,
                entry.mesh.indices.data()
            );
        }
        if (entry.pVertexBuffer == nullptr)
        {
            entry.pVertexBuffer = getDevice()->createStructuredBuffer(
                sizeof(GMesh::Vertex), //
                entry.mesh.getVertexCount(),
                ResourceBindFlags::Vertex,
                MemoryType::DeviceLocal,
                entry.mesh.vertices.data()
            );
        }
        if (entry.pTextureIDBuffer == nullptr)
        {
            static_assert(std::same_as<GMesh::TextureID, uint8_t>);
            entry.pTextureIDBuffer = getDevice()->createTypedBuffer(
                ResourceFormat::R8Uint,
                entry.mesh.getPrimitiveCount(),
                ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
                MemoryType::DeviceLocal,
                entry.mesh.textureIDs.data()
            );
        }
        if (entry.pVao == nullptr)
        {
            if (mpVertexLayout == nullptr)
                mpVertexLayout = GMesh::createVertexLayout();
            entry.pVao = Vao::create(
                Vao::Topology::TriangleList, mpVertexLayout, {entry.pVertexBuffer}, entry.pIndexBuffer, ResourceFormat::R32Uint
            );
        }
    }
}
void GScene::renderUI_entry(Gui::Widgets& widget)
{
    if (widget.button("Add Mesh"))
    {
        std::filesystem::path path;
        if (openFileDialog({}, path))
        {
            mEntries.push_back({
                .mesh = GMesh{.path = path},
                .instances = {GScene::Instance{.name = "new"}},
            });
        }
    }

    std::unordered_set<std::size_t> entryRemoveIndexSet;
    for (std::size_t entryID = 0; entryID < mEntries.size(); ++entryID)
    {
        auto& entry = mEntries[entryID];
        ImGui::PushID((int)entryID);

        if (auto meshGrp = widget.group(fmt::format("Mesh {}", entry.mesh.path.filename()), true))
        {
            if (meshGrp.button("Delete"))
                entryRemoveIndexSet.insert(entryID);
            ImGui::SameLine();
            if (meshGrp.button("Reload"))
                entry.markReload();

            meshGrp.text(fmt::format("Path: {}", entry.mesh.path.string()));
            meshGrp.text(fmt::format("Vertex Count: {}", entry.mesh.getVertexCount()));
            meshGrp.text(fmt::format("Index Count: {}", entry.mesh.getIndexCount()));
            meshGrp.text(fmt::format("Texture Count: {}", entry.mesh.getTextureCount()));

            if (meshGrp.button("Add Instance"))
                entry.instances.push_back({.name = "new"});

            std::unordered_set<std::size_t> instanceRemoveIndexSet;
            for (std::size_t instanceID = 0; instanceID < entry.instances.size(); ++instanceID)
            {
                auto& instance = entry.instances[instanceID];
                ImGui::PushID((int)instanceID);

                if (auto instGrp = meshGrp.group(fmt::format("Instance {}", instance.name), true))
                {
                    if (instGrp.button("Delete"))
                        instanceRemoveIndexSet.insert(instanceID);

                    instGrp.textbox("Name", instance.name);
                    instance.transform.renderUI(instGrp);
                }

                ImGui::PopID();
            }
            removeVectorIndices(entry.instances, instanceRemoveIndexSet);
        }

        ImGui::PopID();
    }

    removeVectorIndices(mEntries, entryRemoveIndexSet);
}

void GScene::renderUIImpl(Gui::Widgets& widget)
{
    /* if (auto g = widget.group("Entry", true))
        renderUI_entry(g); */
    renderUI_entry(widget);
}

void GScene::update()
{
    update_makeUnique();
    update_loadMesh();
    update_loadTexture();
    update_createBuffer();
}

void GScene::draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const
{
    const auto& var = pRasterPass->getRootVar()["gGScene"];
    var["sampler"] = mpSampler;
    mpCamera->bindShaderData(var["camera"]);

    pRasterPass->getState()->setFbo(pFbo);
    pRasterPass->getState()->setRasterizerState(mpRasterState);

    for (const auto& entry : mEntries)
    {
        for (uint i = 0; i < entry.pTextures.size(); ++i)
            var["textures"][i] = entry.pTextures[i];
        for (uint i = 0; i < entry.instances.size(); ++i)
            entry.instances[i].transform.bindShaderData(var["transforms"][i]);
        var["textureIDs"] = entry.pTextureIDBuffer;

        pRasterPass->getState()->setVao(entry.pVao);
        pRenderContext->drawIndexedInstanced(
            pRasterPass->getState().get(), pRasterPass->getVars().get(), entry.mesh.getIndexCount(), entry.instances.size(), 0, 0, 0
        );
    }
}

/* ref<GScene> GScene::create(const ref<Device>& pDevice, const GSceneData& data)
{
    std::vector<MeshView> meshViews;

    std::vector<GMesh::Vertex> vertices;
    std::vector<GMesh::Index> indices;
    std::vector<GMesh::TextureID> textureIDs;
    std::vector<GTransform> transforms;
    std::vector<std::filesystem::path> texturePaths;

    GBound bound{};

    for (const auto& entry : data.entries)
    {
        MeshView meshView = {
            .indexCount = entry.mesh.getIndexCount(),
            .instanceCount = (uint)entry.instances.size(),
            .startIndexLocation = (uint)indices.size(),
            .startInstanceLocation = (uint)transforms.size(),
            .baseIndex = (uint)vertices.size(),
            .basePrimitive = (uint)indices.size() / 3u,
            .baseTextureID = (uint)texturePaths.size(),
        };
        meshViews.push_back(meshView);

        vertices.insert(vertices.end(), entry.mesh.vertices.begin(), entry.mesh.vertices.end());
        auto meshIndices =
            entry.mesh.indices | std::views::transform([=](GMesh::Index x) -> GMesh::Index { return x + meshView.baseIndex; });
        indices.insert(indices.end(), meshIndices.begin(), meshIndices.end());
        auto meshTextureIDs = entry.mesh.textureIDs |
                              std::views::transform([=](GMesh::TextureID x) -> GMesh::TextureID { return x + meshView.baseTextureID; });
        textureIDs.insert(textureIDs.end(), meshTextureIDs.begin(), meshTextureIDs.end());
        texturePaths.insert(texturePaths.end(), entry.mesh.texturePaths.begin(), entry.mesh.texturePaths.end());
        for (const auto& instance : entry.instances)
        {
            bound.merge(instance.transform.apply(entry.mesh.bound));
            transforms.push_back(instance.transform);
        }
    }

    if (texturePaths.size() > GMesh::kMaxTextureID + 1)
    {
        logError("Too many textures.");
        return nullptr;
    }
} */

} // namespace GSGI