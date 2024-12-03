//
// Created by adamyuan on 12/1/24.
//

#pragma once
#ifndef GSGI_GSTATICSCENE_HPP
#define GSGI_GSTATICSCENE_HPP

#include <Falcor.h>
#include <span>
#include <concepts>
#include "../Common/GDeviceObject.hpp"
#include "GScene.hpp"

using namespace Falcor;

namespace GSGI
{

class GStaticScene final : public GDeviceObject<GStaticScene>
{
public:
    struct ImportEntry
    {
        const GMesh* pMesh;
        std::vector<const GTransform*> pInstances;
        std::vector<ref<Texture>> pTextures;
    };

private:
    ref<GScene> mpScene;
    GScene::EntryVersion mSceneEntryVersion{};
    const void* mpCustomizer{};

    std::vector<ImportEntry> getImportEntriesFromScene() const;
    void import(std::span<const ImportEntry> importEntries);

public:
    GStaticScene(ref<Device> pDevice, ref<GScene> pScene) : GDeviceObject(std::move(pDevice)), mpScene{std::move(pScene)} {}
    ~GStaticScene() override = default;

    const auto& getScene() const { return mpScene; }

    void update()
    {
        if (mSceneEntryVersion == mpScene->getEntryVersion() && mpCustomizer == nullptr)
            return;
        import(getImportEntriesFromScene());
        mSceneEntryVersion = mpScene->getEntryVersion();
        mpCustomizer = nullptr;
    }
    void update(const void* pCustomizer, std::invocable<std::size_t, const GMesh*> auto&& customizeMesh)
    {
        FALCOR_CHECK(pCustomizer != nullptr, "Customized import must have a valid customizer pointer");
        if (mSceneEntryVersion == mpScene->getEntryVersion() && mpCustomizer == pCustomizer)
            return;
        auto importEntries = getImportEntriesFromScene();
        for (std::size_t i = 0; i < importEntries.size(); ++i)
            importEntries[i].pMesh = customizeMesh(i, importEntries[i].pMesh);
        import(importEntries);
        mSceneEntryVersion = mpScene->getEntryVersion();
        mpCustomizer = pCustomizer;
    }
};

} // namespace GSGI

#endif // GSGI_GSTATICSCENE_HPP
