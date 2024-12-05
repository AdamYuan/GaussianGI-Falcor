//
// Created by adamyuan on 12/1/24.
//

#include "GStaticScene.hpp"

#include <ranges>

namespace GSGI
{

void GStaticScene::import(const ref<GScene>& pScene, std::span<const GMesh::Ptr> pMeshes)
{
    FALCOR_CHECK(pScene->getEntries().size() == pMeshes.size(), "pMeshes should be of same size as pScene->getEntries()");

    mMeshViews.resize(pMeshes.size());
    for (std::size_t meshID = 0; meshID < pMeshes.size(); ++meshID)
    {
        mMeshViews[meshID] = {
            .pMesh = pMeshes[meshID],
        };
    }
}

std::vector<GMesh::Ptr> GStaticScene::getSceneMeshes(const ref<GScene>& pScene)
{
    std::vector<GMesh::Ptr> meshes;
    meshes.reserve(pScene->getEntries().size());
    for (const auto& entry : pScene->getEntries())
        meshes.push_back(entry.pMesh);
    return meshes;
}

} // namespace GSGI