//
// Created by adamyuan on 12/3/24.
//

#pragma once
#ifndef GSGI_GSCENEOBJECT_HPP
#define GSGI_GSCENEOBJECT_HPP

#include <Falcor.h>
#include "../Common/GDeviceObject.hpp"
#include "GScene.hpp"

using namespace Falcor;

namespace GSGI
{

template<typename Derived_T>
class GSceneObject : public GDeviceObject<Derived_T>
{
private:
    ref<GScene> mpScene;

    GScene::Version mSceneVersion{};

public:
    explicit GSceneObject(const ref<GScene>& pScene) : GDeviceObject<Derived_T>(pScene->getDevice()), mpScene{pScene} {}
    ~GSceneObject() override = default;

    const auto& getScene() const { return mpScene; }

    template<typename... Args>
    void update(Args&&... args)
    {
        static constexpr bool hasUpdateImpl = requires(Derived_T& t) { t.updateImpl(false, std::forward<Args>(args)...); };
        static constexpr bool hasUpdateHasInstanceImpl =
            requires(Derived_T& t) { t.updateHasInstanceImpl(false, std::forward<Args>(args)...); };

        static_assert(hasUpdateImpl != hasUpdateHasInstanceImpl); // Only one should be true

        if constexpr (hasUpdateHasInstanceImpl)
        {
            if (!mpScene->hasInstance())
                return;
        }

        bool isSceneChanged = false;
        if (mSceneVersion != mpScene->getVersion())
            isSceneChanged = true;

        mSceneVersion = mpScene->getVersion();

        if constexpr (hasUpdateImpl)
            static_cast<Derived_T*>(this)->updateImpl(isSceneChanged, std::forward<Args>(args)...);

        if constexpr (hasUpdateHasInstanceImpl)
            static_cast<Derived_T*>(this)->updateHasInstanceImpl(isSceneChanged, std::forward<Args>(args)...);
    }
};

} // namespace GSGI

#endif // GSGI_GSCENEOBJECT_HPP