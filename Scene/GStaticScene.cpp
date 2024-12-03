//
// Created by adamyuan on 12/1/24.
//

#include "GStaticScene.hpp"

#include <ranges>

namespace GSGI
{

void GStaticScene::import(std::span<const ImportEntry> importEntries) {}

std::vector<GStaticScene::ImportEntry> GStaticScene::getImportEntriesFromScene() const
{
    std::vector<ImportEntry> importEntries;
    for (const auto& entry : mpScene->getEntries())
    {
        FALCOR_CHECK(entry.mesh.isLoaded(), "must be called after GScene::update()");
        auto pInstances = entry.instances | std::views::transform([](const GScene::Instance& inst) { return &inst.transform; });
        importEntries.push_back({
            .pMesh = &entry.mesh,
            .pInstances = {pInstances.begin(), pInstances.end()},
            .pTextures = entry.pTextures,
        });
    }
    return importEntries;
}

} // namespace GSGI