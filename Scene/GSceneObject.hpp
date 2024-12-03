//
// Created by adamyuan on 12/3/24.
//

#pragma once
#ifndef GSGI_GSCENEOBJECT_HPP
#define GSGI_GSCENEOBJECT_HPP

#include <Falcor.h>
#include "../Common/GDeviceObject.hpp"
#include "GStaticScene.hpp"

using namespace Falcor;

namespace GSGI
{

template<typename Derived_T, bool IsMeshCustomized = false>
class GSceneObject : public GDeviceObject<Derived_T>
{
private:
    ref<GStaticScene> mpStaticScene;
    ref<GScene> mpScene;

    GScene::EntryVersion mSceneEntryVersion{};

    std::conditional_t<IsMeshCustomized, std::vector<GMesh>, std::monostate> mCustomizeMeshes;

protected:
    void setCustomizedMesh(std::size_t idx, GMesh&& mesh)
        requires IsMeshCustomized
    {
        mCustomizeMeshes[idx] = std::move(mesh);
    }

public:
    explicit GSceneObject(const ref<GStaticScene>& pStaticScene)
        : GDeviceObject<Derived_T>(pStaticScene->getDevice()), mpStaticScene{pStaticScene}, mpScene{pStaticScene->getScene()}
    {}

    const auto& getScene() const { return mpScene; }
    const auto& getStaticScene() const { return mpStaticScene; }

    // Only one GSceneObject belonging to a GStaticScene should be updated per-frame
    template<typename... Args>
    void update(Args&&... args)
    {
        // Reload when GScene changed
        if (mSceneEntryVersion != mpScene->getEntryVersion())
        {
            if constexpr (IsMeshCustomized)
            {
                mCustomizeMeshes.clear();
                mCustomizeMeshes.resize(mpScene->getEntries().size());
            }
            static_cast<Derived_T*>(this)->reloadImpl();
            mSceneEntryVersion = mpScene->getEntryVersion();
        }

        // Update GStaticScene
        if constexpr (IsMeshCustomized)
            mpStaticScene->update(
                this,
                [this](std::size_t idx, const GMesh* pMesh)
                {
                    if (mCustomizeMeshes[idx].isLoaded())
                        return mCustomizeMeshes.data() + idx;
                    return pMesh;
                }
            );
        else
            mpStaticScene->update();

        static_cast<Derived_T*>(this)->updateImpl(std::forward<Args>(args)...);
    }
};

} // namespace GSGI

#endif // GSGI_GSCENEOBJECT_HPP