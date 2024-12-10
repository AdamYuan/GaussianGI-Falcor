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
public:
    struct MeshInfo
    {
        uint indexCount;
        uint instanceCount;
        uint firstIndex;
        uint firstInstance;
    };
    struct InstanceInfo
    {
        GTransform transform;
        uint meshID{};
    };
    struct MeshView
    {
        GMesh::Ptr pMesh;
        MeshInfo info;
    };

private:
    void import(const ref<GScene>& pScene, std::span<const GMesh::Ptr> pMeshes);
    void buildBLAS(RenderContext* pRenderContext);
    void buildTLAS(RenderContext* pRenderContext);

    std::vector<MeshView> mMeshViews;
    std::vector<InstanceInfo> mInstanceInfos;
    ref<Buffer> mpIndexBuffer, mpVertexBuffer, mpTextureIDBuffer, mpDrawCmdBuffer, mpInstanceInfoBuffer, mpMeshInfoBuffer;
    std::vector<ref<Texture>> mpTextures;
    ref<Vao> mpVao;

    bool isAccelStructBuilt = false;
    ref<RtAccelerationStructure> mpTLAS;
    std::vector<ref<RtAccelerationStructure>> mpMeshBLASs;
    ref<Buffer> mpBLASBuffer, mpTLASBuffer;

    ref<GScene> mpScene; // for getCamera() and getLighting() in bindRootShaderData()

    static std::vector<GMesh::Ptr> getSceneMeshes(const ref<GScene>& pScene);

public:
    GStaticScene(const ref<GScene>& pScene, std::span<const GMesh::Ptr> pAlternateMeshes)
        : GDeviceObject(pScene->getDevice()), mpScene{pScene}
    {
        import(pScene, pAlternateMeshes);
    }
    explicit GStaticScene(const ref<GScene>& pScene) : GStaticScene(pScene, getSceneMeshes(pScene)) {}
    ~GStaticScene() override = default;

    const auto& getScene() const { return mpScene; }

    uint getMeshCount() const { return mMeshViews.size(); }
    const auto& getMeshViews() const { return mMeshViews; }
    const MeshView& getMeshView(std::size_t meshID) const { return mMeshViews[meshID]; }

    void bindRootShaderData(const ShaderVar& rootVar) const;

    void update(RenderContext* pRenderContext)
    {
        if (isAccelStructBuilt == false)
        {
            buildBLAS(pRenderContext);
            buildTLAS(pRenderContext);
            isAccelStructBuilt = true;
        }
    }
    void draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const;
};

} // namespace GSGI

#endif // GSGI_GSTATICSCENE_HPP
