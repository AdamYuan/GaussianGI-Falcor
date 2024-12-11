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
    struct MeshEntry
    {
        GMesh::Ptr pMesh;
        std::vector<Instance> instances;

        ref<Vao> pVao;
        ref<Buffer> pVertexBuffer, pIndexBuffer, pTextureIDBuffer;
    };
    using Version = uint64_t;

private:
    ref<VertexLayout> mpVertexLayout;
    ref<Texture> mpDefaultTexture;
    ref<RasterPass> mpDefaultRasterPass;
    ref<RasterizerState> mpDefaultRasterState;

    std::vector<MeshEntry> mMeshEntries;
    Version mVersion{};
    std::size_t mInstanceCount{};

    ref<Camera> mpCamera;
    ref<GLighting> mpLighting;

    void update_countInstance();
    void update_makeUnique();
    void update_createBuffer();

    void renderUI_entry(Gui::Widgets& widget, bool& modified);

public:
    explicit GScene(ref<Device> pDevice);
    ~GScene() override = default;

    const auto& getMeshEntries() const { return mMeshEntries; }
    Version getVersion() const { return mVersion; }

    std::size_t getInstanceCount() const { return mInstanceCount; }
    bool hasInstance() const { return mInstanceCount > 0; }

    void setCamera(ref<Camera> pCamera) { mpCamera = std::move(pCamera); }
    const auto& getCamera() const { return mpCamera; }

    void setLighting(ref<GLighting> pLighting) { mpLighting = std::move(pLighting); }
    const auto& getLighting() const { return mpLighting; }

    const auto& getVertexLayout() const { return mpVertexLayout; }
    const auto& getDefaultRasterState() const { return mpDefaultRasterState; }

    void renderUIImpl(Gui::Widgets& widget);

    void update();

    void draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const;
    void draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo) const { draw(pRenderContext, pFbo, mpDefaultRasterPass); }
};

} // namespace GSGI

#endif // GSGI_GSCENE_HPP
