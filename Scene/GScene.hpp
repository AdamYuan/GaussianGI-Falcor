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
    ref<Sampler> mpSampler;
    ref<RasterPass> mpDefaultRasterPass;
    ref<RasterizerState> mpRasterState;

    std::vector<Entry> mEntries;

    ref<Camera> mpCamera;

    void update_makeUnique();
    void update_loadMesh();
    void update_loadTexture();
    void update_createBuffer();

    void renderUI_entry(Gui::Widgets& widget);

public:
    explicit GScene(ref<Device> pDevice);
    ~GScene() override = default;
    GScene(ref<Device> pDevice, std::vector<Entry>&& entries) : GDeviceObject(std::move(pDevice)), mEntries{std::move(entries)} {}

    const auto& getEntries() const { return mEntries; }
    auto& getEntries() { return mEntries; }

    void setCamera(ref<Camera> pCamera) { mpCamera = std::move(pCamera); }
    const auto& getCamera() const { return mpCamera; }

    void renderUIImpl(Gui::Widgets& widget);

    void update();

    void draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const;
    void draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo) const { draw(pRenderContext, pFbo, mpDefaultRasterPass); }
};

} // namespace GSGI

#endif // GSGI_GSCENE_HPP
