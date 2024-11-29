//
// Created by adamyuan on 11/25/24.
//

#pragma once
#ifndef GSGI_GSCENE_HPP
#define GSGI_GSCENE_HPP

#include <Falcor.h>

#include "GMesh.hpp"
#include "GTransform.hpp"
#include "../Common/GDeviceObject.hpp"

using namespace Falcor;

namespace GSGI
{

class GScene final : public GDeviceObject<GScene>
{
public:
    struct Instance
    {
        std::string name;
        GTransform transform;
    };
    struct Entry
    {
        GMesh mesh;
        std::vector<Instance> instances;

        ref<Vao> vao;
        ref<Buffer> vertexBuffer, indexBuffer, textureIDBuffer;
        std::vector<ref<Texture>> textures;

        void markReload()
        {
            auto path = std::move(this->mesh.path);
            auto instances = std::move(this->instances);
            *this = {
                .mesh = {.path = std::move(path)},
                .instances = std::move(instances),
            };
        }
        // bool isReady() const { return vao && vertexBuffer && indexBuffer && textureIDBuffer && !textures.empty(); }
    };

private:
    ref<VertexLayout> mpVertexLayout;
    ref<Texture> mpDefaultTexture;
    std::vector<Entry> mEntries;

    void update_makeUnique();
    void update_loadMesh();
    void update_loadTexture();
    void update_createBuffer();

public:
    explicit GScene(ref<Device> pDevice) : GDeviceObject(std::move(pDevice)) {}
    GScene(ref<Device> pDevice, std::vector<Entry>&& entries) : GDeviceObject(std::move(pDevice)), mEntries{std::move(entries)} {}

    const auto& getEntries() const { return mEntries; }
    // auto& getEntries() { return mEntries; }

    void renderUIImpl(Gui::Widgets& widget);

    void update();
};

} // namespace GSGI

#endif // GSGI_GSCENE_HPP
