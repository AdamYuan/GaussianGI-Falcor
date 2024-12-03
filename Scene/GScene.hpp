//
// Created by adamyuan on 11/25/24.
//

#pragma once
#ifndef GSGI_GSCENE_HPP
#define GSGI_GSCENE_HPP

#include <Falcor.h>
#include <Scene/Camera/Camera.h>
#include <Core/Pass/RasterPass.h>
#include "GMesh.hpp"
#include "GTransform.hpp"
#include "GLighting.hpp"
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

        ref<Vao> pVao;
        ref<Buffer> pVertexBuffer, pIndexBuffer, pTextureIDBuffer;
        std::vector<ref<Texture>> pTextures;
    };
    using EntryVersion = uint64_t;

private:
    ref<VertexLayout> mpVertexLayout;
    ref<Texture> mpDefaultTexture;
    ref<RasterPass> mpDefaultRasterPass;
    ref<RasterizerState> mpRasterState;

    std::vector<Entry> mEntries;
    EntryVersion mEntryVersion{};

    ref<Camera> mpCamera;
    ref<GLighting> mpLighting;

    static void entry_markReload(Entry& entry)
    {
        auto path = std::move(entry.mesh.path);
        auto instances = std::move(entry.instances);
        entry = {
            .mesh = {.path = std::move(path)},
            .instances = std::move(instances),
        };
    }

    void update_makeUnique();
    void update_loadMesh();
    void update_loadTexture();
    void update_createBuffer();

    void renderUI_entry(Gui::Widgets& widget, bool& modified);

public:
    explicit GScene(ref<Device> pDevice);
    ~GScene() override = default;
    GScene(ref<Device> pDevice, std::vector<Entry>&& entries) : GScene(std::move(pDevice)) { setEntries(std::move(entries)); }

    void setEntries(std::vector<Entry>&& entries)
    {
        mEntries = std::move(entries);
        ++mEntryVersion;
    }
    const auto& getEntries() const { return mEntries; }
    EntryVersion getEntryVersion() const { return mEntryVersion; }

    void setCamera(ref<Camera> pCamera) { mpCamera = std::move(pCamera); }
    const auto& getCamera() const { return mpCamera; }

    void setLighting(ref<GLighting> pLighting) { mpLighting = std::move(pLighting); }
    const auto& getLighting() const { return mpLighting; }

    void renderUIImpl(Gui::Widgets& widget);

    void update();

    void draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const;
    void draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo) const { draw(pRenderContext, pFbo, mpDefaultRasterPass); }
};

} // namespace GSGI

#endif // GSGI_GSCENE_HPP
