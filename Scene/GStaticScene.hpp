//
// Created by adamyuan on 12/1/24.
//

#pragma once
#ifndef GSGI_GSTATICSCENE_HPP
#define GSGI_GSTATICSCENE_HPP

#include <Falcor.h>
#include <span>
#include "GScene.hpp"
#include "../GDeviceObject.hpp"

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

    static constexpr uint32_t kMaxInstanceCount = GScene::kMaxInstanceCount;

private:
    void import(std::vector<ref<GMesh>>&& pMeshes);
    void buildBLAS(RenderContext* pRenderContext);
    void buildTLAS(RenderContext* pRenderContext);

    std::vector<ref<GMesh>> mpMeshes;
    std::vector<MeshInfo> mMeshInfos;
    std::vector<InstanceInfo> mInstanceInfos;
    ref<Buffer> mpIndexBuffer, mpVertexBuffer, mpTextureIDBuffer, mpDrawCmdBuffer, mpInstanceInfoBuffer, mpMeshInfoBuffer;
    std::vector<ref<Texture>> mpTextures;
    ref<Vao> mpVao;

    ref<RtAccelerationStructure> mpTLAS;
    std::vector<ref<RtAccelerationStructure>> mpMeshBLASs;

    ref<GScene> mpScene; // for getCamera() and getLighting() in bindRootShaderData()

    static std::vector<ref<GMesh>> getSceneMeshes(const ref<GScene>& pScene);

public:
    GStaticScene(const ref<GScene>& pScene, RenderContext* pRenderContext, std::vector<ref<GMesh>>&& pAlternateMeshes)
        : GDeviceObject(pScene->getDevice()), mpScene{pScene}
    {
        import(std::move(pAlternateMeshes));
        buildBLAS(pRenderContext);
        buildTLAS(pRenderContext);
    }
    explicit GStaticScene(const ref<GScene>& pScene, RenderContext* pRenderContext)
        : GStaticScene(pScene, pRenderContext, getSceneMeshes(pScene))
    {}
    ~GStaticScene() override = default;

    const auto& getScene() const { return mpScene; }

    uint getMeshCount() const { return mpMeshes.size(); }
    const auto& getMeshes() const { return mpMeshes; }
    const auto& getMeshInfos() const { return mMeshInfos; }
    uint getInstanceCount() const { return mInstanceInfos.size(); }
    const auto& getInstanceInfos() const { return mInstanceInfos; }

    void bindRootShaderData(const ShaderVar& rootVar) const;

    void draw(RenderContext* pRenderContext, const ref<Fbo>& pFbo, const ref<RasterPass>& pRasterPass) const;
};

} // namespace GSGI

#endif // GSGI_GSTATICSCENE_HPP
