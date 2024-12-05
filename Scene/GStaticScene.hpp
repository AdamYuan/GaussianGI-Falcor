//
// Created by adamyuan on 12/1/24.
//

#pragma once
#ifndef GSGI_GSTATICSCENE_HPP
#define GSGI_GSTATICSCENE_HPP

#include <Falcor.h>
#include <span>
#include "GScene.hpp"
#include "../Common/GDeviceObject.hpp"

using namespace Falcor;

namespace GSGI
{

class GStaticScene final : public GDeviceObject<GStaticScene>
{
private:
    void import(const ref<GScene>& pScene, std::span<const GMesh::Ptr> pMeshes);

    struct MeshView
    {
        GMesh::Ptr pMesh;
    };
    std::vector<MeshView> mMeshViews;
    ref<Buffer> mpIndexBuffer, mpVertexBuffer, mpTransformBuffer, mpTextureIDBuffer;
    std::vector<ref<Texture>> mpTextures;

    static std::vector<GMesh::Ptr> getSceneMeshes(const ref<GScene>& pScene);

public:
    GStaticScene(const ref<GScene>& pScene, std::span<const GMesh::Ptr> pAlternateMeshes) : GDeviceObject(pScene->getDevice())
    {
        import(pScene, pAlternateMeshes);
    }
    explicit GStaticScene(const ref<GScene>& pScene) : GStaticScene(pScene, getSceneMeshes(pScene)) {}
    ~GStaticScene() override = default;

    uint getMeshCount() const { return mMeshViews.size(); }
    const auto& getMeshViews() const { return mMeshViews; }
    const MeshView& getMeshView(std::size_t meshID) const { return mMeshViews[meshID]; }

    // GStaticScene::update() must be called after GScene::update()
    /* void updateImpl(bool isSceneChanged)
    {
        FALCOR_CHECK(getScene()->hasInstance(), "GScene::hasInstance() must be true");
        if (!isSceneChanged && mOwnerTag == nullptr)
            return;
        auto importEntries = getImportEntriesFromScene();
        import(importEntries);
        mOwnerTag = nullptr;
    }
    void updateImpl(bool isSceneChanged, const void* pOwner, std::invocable<std::size_t, const GMesh*> auto&& customizeMesh)
    {
        FALCOR_CHECK(getScene()->hasInstance(), "GScene::hasInstance() must be true");
        FALCOR_CHECK(pOwner != nullptr, "Customized import must have a valid customizer pointer");
        if (!isSceneChanged && mOwnerTag == pOwner)
            return;
        auto importEntries = getImportEntriesFromScene();
        for (std::size_t i = 0; i < importEntries.size(); ++i)
            importEntries[i].pMesh = customizeMesh(i, importEntries[i].pMesh);
        import(importEntries);
        mOwnerTag = pOwner;
    } */
};

} // namespace GSGI

#endif // GSGI_GSTATICSCENE_HPP
