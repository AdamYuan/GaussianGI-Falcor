//
// Created by adamyuan on 12/3/24.
//

#pragma once
#ifndef GSGI_GSCENEOBJECT_HPP
#define GSGI_GSCENEOBJECT_HPP

#include <Falcor.h>
#include "../GDeviceObject.hpp"
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
        bool isSceneChanged = false;
        if (mSceneVersion != mpScene->getVersion())
            isSceneChanged = true;

        mSceneVersion = mpScene->getVersion();

        static_cast<Derived_T*>(this)->updateImpl(isSceneChanged, std::forward<Args>(args)...);
    }
};

} // namespace GSGI

#endif // GSGI_GSCENEOBJECT_HPP