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
    ProgramDesc defaultRasterDesc;
    defaultRasterDesc.addShaderLibrary("GaussianGI/Scene/GScene.3d.slang").vsEntry("vsMain").gsEntry("gsMain").psEntry("psMain");
    mpDefaultRasterPass = RasterPass::create(getDevice(), defaultRasterDesc);
    RasterizerState::Desc rasterStateDesc = {};
    rasterStateDesc.setCullMode(RasterizerState::CullMode::None);
    mpDefaultRasterState = RasterizerState::create(rasterStateDesc);
    mpDefaultRasterPass->getState()->setRasterizerState(mpDefaultRasterState);
}

static void removeVectorIndices(auto& vector, auto& indexSet)
{
    if (!indexSet.empty())
        vector.erase(
            std::remove_if(vector.begin(), vector.end(), [&](const auto& e) { return indexSet.contains(&e - vector.data()); }), vector.end()
        );
}

void GScene::update_countInstance()
{
    mInstanceCount = 0;
    for (const auto& entry : mMeshEntries)
        mInstanceCount += entry.instances.size();
}

void GScene::update_makeUnique()
{
    std::unordered_map<std::filesystem::path, std::size_t> meshPathIndexMap;
    std::unordered_set<std::size_t> removeIndexSet;
    for (std::size_t i = 0; i < mMeshEntries.size(); ++i)
    {
        auto& entry = mMeshEntries[i];
        auto mapIt = meshPathIndexMap.find(entry.pMesh->path);
        if (mapIt == meshPathIndexMap.end())
        {
            meshPathIndexMap[entry.pMesh->path] = i;
            continue;
        }
        auto& uniqueEntry = mMeshEntries[mapIt->second];
        // Move instances to unique entry
        uniqueEntry.instances.insert(
            uniqueEntry.instances.end(), //
            std::make_move_iterator(entry.instances.begin()),
            std::make_move_iterator(entry.instances.end())
        );
        removeIndexSet.insert(i);
    }

    removeVectorIndices(mMeshEntries, removeIndexSet);
}

void GScene::update_createBuffer()
{
    for (auto& entry : mMeshEntries)
    {
        const auto& pMesh = entry.pMesh;

        if (entry.pIndexBuffer == nullptr)
        {
            static_assert(std::same_as<GMesh::Index, uint32_t>);
            entry.pIndexBuffer = getDevice()->createStructuredBuffer(
                sizeof(GMesh::Index), //
                pMesh->getIndexCount(),
                ResourceBindFlags::Index,
                MemoryType::DeviceLocal,
                pMesh->indices.data()
            );
        }
        if (entry.pVertexBuffer == nullptr)
        {
            entry.pVertexBuffer = getDevice()->createStructuredBuffer(
                sizeof(GMesh::Vertex), //
                pMesh->getVertexCount(),
                ResourceBindFlags::Vertex,
                MemoryType::DeviceLocal,
                pMesh->vertices.data()
            );
        }
        if (entry.pTextureIDBuffer == nullptr)
        {
            static_assert(std::same_as<GMesh::TextureID, uint8_t>);
            entry.pTextureIDBuffer = getDevice()->createTypedBuffer(
                ResourceFormat::R8Uint,
                pMesh->getPrimitiveCount(),
                ResourceBindFlags::ShaderResource,
                MemoryType::DeviceLocal,
                pMesh->textureIDs.data()
            );
        }
        if (entry.pVao == nullptr)
        {
            if (mpVertexLayout == nullptr)
                mpVertexLayout = GMesh::createVertexLayout();
            entry.pVao = Vao::create(
                Vao::Topology::TriangleList, mpVertexLayout, {entry.pVertexBuffer}, entry.pIndexBuffer, GMesh::getIndexFormat()
            );
        }
    }
}

void GScene::renderUI_entry(Gui::Widgets& widget, bool& modified)
{
    const auto loadMesh = [this](const std::filesystem::path& path, std::invocable<GMesh::Ptr&&> auto&& onSuccess)
    {
        auto pMesh = GMeshLoader::load(getDevice(), path);
        if (pMesh == nullptr)
            logWarning("Failed to load mesh {}", path);
        else
        {
            logInfo("Loaded mesh {}", path);
            onSuccess(std::move(pMesh));
        }
    };

    widget.text(fmt::format("Version: {}", mVersion));
    widget.text(fmt::format("Instance Count: {}", mInstanceCount));

    if (widget.button("Add Mesh"))
    {
        std::filesystem::path path;
        if (openFileDialog({}, path))
        {
            modified = true;
            loadMesh(
                path,
                [&](GMesh::Ptr&& pMesh)
                {
                    mMeshEntries.push_back({
                        .pMesh = std::move(pMesh),
                        .instances = {{.name = "new"}},
                    });
                }
            );
        }
    }

    std::unordered_set<std::size_t> entryRemoveIndexSet;
    for (std::size_t entryID = 0; entryID < mMeshEntries.size(); ++entryID)
    {
        auto& entry = mMeshEntries[entryID];
        ImGui::PushID((int)entryID);

        if (auto meshGrp = widget.group(fmt::format("Mesh {}", entry.pMesh->path.filename()), true))
        {
            if (meshGrp.button("Delete"))
            {
                modified = true;
                entryRemoveIndexSet.insert(entryID);
            }
            ImGui::SameLine();
            if (meshGrp.button("Reload"))
            {
                modified = true;
                loadMesh(entry.pMesh->path, [&](GMesh::Ptr&& pMesh) { entry.pMesh = std::move(pMesh); });
            }

            meshGrp.text(fmt::format("Path: {}", entry.pMesh->path.string()));
            meshGrp.text(fmt::format("Vertex Count: {}", entry.pMesh->getVertexCount()));
            meshGrp.text(fmt::format("Index Count: {}", entry.pMesh->getIndexCount()));
            meshGrp.text(fmt::format("Texture Count: {}", entry.pMesh->getTextureCount()));

            if (meshGrp.button("Add Instance"))
            {
                modified = true;
                entry.instances.push_back({.name = "new"});
            }

            std::unordered_set<std::size_t> instanceRemoveIndexSet;
            for (std::size_t instanceID = 0; instanceID < entry.instances.size(); ++instanceID)
            {
                auto& instance = entry.instances[instanceID];
                ImGui::PushID((int)instanceID);

                if (auto instGrp = meshGrp.group(fmt::format("Instance #{}", instanceID), true))
                {
                    if (instGrp.button("Delete"))
                    {
                        modified = true;
                        instanceRemoveIndexSet.insert(instanceID);
                    }

                    instGrp.textbox("Name", instance.name);
                    if (instance.transform.renderUI(instGrp))
                        modified = true;
                }

                ImGui::PopID();
            }
            removeVectorIndices(entry.instances, instanceRemoveIndexSet);
        }

        ImGui::PopID();
    }

    removeVectorIndices(mMeshEntries, entryRemoveIndexSet);
}

void GScene::renderUIImpl(Gui::Widgets& widget)
{
    /* if (auto g = widget.group("Entry", true))
        renderUI_entry(g); */
    bool entryModified = false;
    renderUI_entry(widget, entryModified);
    if (entryModified)
        ++mVersion;
}

void GScene::update()
{
    update_countInstance();
    update_makeUnique();
    update_createBuffer();
}

void GScene::draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const
{
    const auto& var = pRasterPass->getRootVar()["gGScene"];
    var["sampler"] = getDevice()->getDefaultSampler();
    mpCamera->bindShaderData(var["camera"]);
    mpLighting->bindShaderData(var["lighting"]);

    pRasterPass->getState()->setFbo(pFbo);

    for (const auto& entry : mMeshEntries)
    {
        for (uint i = 0; i < entry.pMesh->textures.size(); ++i)
            var["textures"][i] = entry.pMesh->textures[i].pTexture;
        var["textureIDs"] = entry.pTextureIDBuffer;

        pRasterPass->getState()->setVao(entry.pVao);

        for (const auto& instance : entry.instances)
        {
            instance.transform.bindShaderData(var["transform"]);
            pRenderContext->drawIndexed(pRasterPass->getState().get(), pRasterPass->getVars().get(), entry.pMesh->getIndexCount(), 0, 0);
        }
    }
}

} // namespace GSGI