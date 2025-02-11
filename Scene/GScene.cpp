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
    mpDefaultRasterPass->getState()->setRasterizerState(GMesh::getRasterizerState());
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
    for (auto& entry : mMeshEntries)
    {
        FALCOR_CHECK(mInstanceCount <= kMaxInstanceCount, "");
        // Remove exceeding instances
        if (mInstanceCount + entry.instances.size() > kMaxInstanceCount)
        {
            std::size_t eraseCount = mInstanceCount + entry.instances.size() - kMaxInstanceCount;
            FALCOR_CHECK(eraseCount <= entry.instances.size(), "");
            entry.instances.erase(entry.instances.end() - (int)eraseCount, entry.instances.end());
        }
        mInstanceCount += entry.instances.size();
        FALCOR_CHECK(mInstanceCount <= kMaxInstanceCount, "");
    }
}

void GScene::update_makeUnique()
{
    std::unordered_map<std::filesystem::path, std::size_t> meshPathIndexMap;
    std::unordered_set<std::size_t> removeIndexSet;
    for (std::size_t i = 0; i < mMeshEntries.size(); ++i)
    {
        auto& entry = mMeshEntries[i];
        auto mapIt = meshPathIndexMap.find(entry.pMesh->getSourcePath());
        if (mapIt == meshPathIndexMap.end())
        {
            meshPathIndexMap[entry.pMesh->getSourcePath()] = i;
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

void GScene::renderUI_entry(Gui::Widgets& widget, bool& modified)
{
    bool canAddInstance = mInstanceCount < kMaxInstanceCount;

    const auto loadMesh = [this](const std::filesystem::path& path, std::invocable<ref<GMesh>&&> auto&& onSuccess)
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

    if (!canAddInstance)
        ImGui::BeginDisabled();
    if (widget.button("Add Mesh"))
    {
        std::filesystem::path path;
        if (openFileDialog({}, path))
        {
            modified = true;
            loadMesh(
                path,
                [&](ref<GMesh>&& pMesh)
                {
                    mMeshEntries.push_back({
                        .pMesh = std::move(pMesh),
                        .instances = {{.name = "new"}},
                    });
                }
            );
        }
    }
    if (!canAddInstance)
        ImGui::EndDisabled();

    std::unordered_set<std::size_t> entryRemoveIndexSet;
    for (std::size_t entryID = 0; entryID < mMeshEntries.size(); ++entryID)
    {
        auto& entry = mMeshEntries[entryID];
        ImGui::PushID((int)entryID);

        if (auto meshGrp = widget.group(fmt::format("Mesh {}", entry.pMesh->getSourcePath().filename()), true))
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
                loadMesh(entry.pMesh->getSourcePath(), [&](ref<GMesh>&& pMesh) { entry.pMesh = std::move(pMesh); });
            }

            entry.pMesh->renderUI(meshGrp);

            if (!canAddInstance)
                ImGui::BeginDisabled();
            if (meshGrp.button("Add Instance"))
            {
                modified = true;
                entry.instances.push_back({.name = "new"});
            }
            if (!canAddInstance)
                ImGui::EndDisabled();

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
}

void GScene::draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const
{
    const auto& var = pRasterPass->getRootVar()["gGScene"];
    var["sampler"] = getDevice()->getDefaultSampler();
    mpCamera->bindShaderData(var["camera"]);
    mpLighting->bindShaderData(var["lighting"]);

    pRasterPass->getState()->setFbo(pFbo);

    for (const auto& entry : mMeshEntries)
        for (const auto& instance : entry.instances)
        {
            instance.transform.bindShaderData(var["transform"]);
            entry.pMesh->draw(pRenderContext, pRasterPass, var["rasterData"]);
        }
}

} // namespace GSGI