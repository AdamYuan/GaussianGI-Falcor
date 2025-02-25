add_falcor_executable(GaussianGI)
set_target_properties(GaussianGI PROPERTIES CXX_STANDARD 20)
target_sources(GaussianGI PRIVATE
        GaussianGI.cpp
        GaussianGI.h
        GDeviceObject.hpp

        Util/DeviceUtil.hpp
        Util/DeviceUtil.cpp
        Util/ShaderUtil.hpp
        Util/TextureUtil.hpp
        Util/EnumUtil.hpp
        Util/SerializeUtil.hpp
        Util/SOABufferUtil.hpp
        Util/SOABufferDefine.slangh
        Util/BLASUtil.hpp
        Util/BLASUtil.cpp
        Util/TLASUtil.hpp
        Util/TLASUtil.cpp

        Algorithm/AliasTable.hpp
        Algorithm/AliasTable.cpp
        Algorithm/MeshView.hpp
        Algorithm/MeshSample.hpp
        Algorithm/MeshBVH.hpp
        Algorithm/MeshVHBVH.hpp
        Algorithm/MeshClosestPoint.hpp
        Algorithm/MeshRangeSearch.hpp
        Algorithm/MeshGSOptimize.hpp
        Algorithm/ThreadGroupTilingX.slang
        Algorithm/ShaderMisc.slang
        Algorithm/GS3D.slang
        Algorithm/GS3DBound.hpp
        Algorithm/GS3DBound.slangh
        Algorithm/Icosahedron.hpp
        Algorithm/Icosahedron.slangh
        Algorithm/ShadowMapTransform.hpp
        Algorithm/ShadowMapTransform.slang

        Algorithm/DeviceSort/DeviceSorter.hpp
        Algorithm/DeviceSort/DeviceSorter.cpp
        Algorithm/DeviceSort/DeviceSorter.slangh
        Algorithm/DeviceSort/DeviceSorterSizes.slangh
        Algorithm/DeviceSort/Reset.cs.slang
        Algorithm/DeviceSort/GlobalHist.cs.slang
        Algorithm/DeviceSort/ScanHist.cs.slang
        Algorithm/DeviceSort/OneSweep.cs.slang

        Scene/GScene.hpp
        Scene/GScene.cpp
        Scene/GScene.slang
        Scene/GScene.3d.slang
        Scene/GMesh.hpp
        Scene/GMesh.cpp
        Scene/GMesh.slang
        Scene/GMesh.slangh
        Scene/GMeshView.hpp
        Scene/GMeshLoader.hpp
        Scene/GMeshLoader.cpp
        Scene/GTransform.hpp
        Scene/GTransform.slang
        Scene/GStaticScene.hpp
        Scene/GStaticScene.cpp
        Scene/GStaticSceneAS.cpp
        Scene/GStaticScene.slang
        Scene/GLighting.hpp
        Scene/GLighting.cpp
        Scene/GLighting.slang
        Scene/GSceneObject.hpp

        Renderer/GRenderer.hpp
        Renderer/GRenderer.cpp
        Renderer/GRenderer.cs.slang
        Renderer/GRendererViewType.hpp
        Renderer/GVBuffer.hpp
        Renderer/GVBuffer.cpp
        Renderer/GVBuffer.slang
        Renderer/GVBuffer.3d.slang

        Renderer/Shadow/GShadow.hpp
        Renderer/Shadow/GShadow.cpp
        Renderer/Shadow/GShadow.slang
        Renderer/Shadow/GShadowType.hpp
        Renderer/Shadow/StatelessShadow.hpp

        Renderer/Shadow/RT/RTShadow.slang

        Renderer/Shadow/PCF/PCFShadow.hpp
        Renderer/Shadow/PCF/PCFShadow.cpp

        Renderer/Shadow/EVSM/EVSMShadow.hpp
        Renderer/Shadow/EVSM/EVSMShadow.cpp
        Renderer/Shadow/EVSM/EVSMDraw.3d.slang

        Renderer/IndLight/GIndLight.hpp
        Renderer/IndLight/GIndLight.cpp
        Renderer/IndLight/GIndLightArgs.hpp
        Renderer/IndLight/GIndLightType.hpp
        Renderer/IndLight/NoIndLight.hpp

        Renderer/IndLight/PathTraced/PTIndLight.hpp
        Renderer/IndLight/PathTraced/PTIndLight.cpp
        Renderer/IndLight/PathTraced/PTIndLight.cs.slang

        Renderer/IndLight/GS3D/GS3DIndLight.hpp
        Renderer/IndLight/GS3D/GS3DIndLight.cpp
        Renderer/IndLight/GS3D/GS3DIndLight.cs.slang
        Renderer/IndLight/GS3D/GS3DIndLightAlgo.hpp
        Renderer/IndLight/GS3D/GS3DIndLightAlgo.cpp
        Renderer/IndLight/GS3D/GS3DIndLightSplat.slang
        Renderer/IndLight/GS3D/GS3DIndLightSplat.hpp
        Renderer/IndLight/GS3D/GS3DIndLightSplat.cpp
        Renderer/IndLight/GS3D/GS3DIndLightSplatPersist.cpp
        Renderer/IndLight/GS3D/GS3DIndLightCull.cs.slang
        Renderer/IndLight/GS3D/GS3DIndLightDraw.3d.slang
        Renderer/IndLight/GS3D/GS3DIndLightBlend.cs.slang
        Renderer/IndLight/GS3D/Misc/GS3DMiscRenderer.hpp
        Renderer/IndLight/GS3D/Misc/GS3DMiscRenderer.cpp
        Renderer/IndLight/GS3D/Misc/GS3DMiscPoint.3d.slang
        Renderer/IndLight/GS3D/Misc/GS3DMiscSplatView.cs.slang
        Renderer/IndLight/GS3D/Misc/GS3DMiscSplatDraw.3d.slang
        Renderer/IndLight/GS3D/Misc/GS3DMiscTraceSplat.rt.slang
)
target_link_libraries(GaussianGI PRIVATE assimp tbb)
target_copy_shaders(GaussianGI GaussianGI)
target_source_group(GaussianGI "GaussianGI")

add_falcor_executable(GaussianGITest)
set_target_properties(GaussianGITest PROPERTIES CXX_STANDARD 20)
target_sources(GaussianGITest PRIVATE
        GaussianGITest.cpp

        Util/DeviceUtil.hpp
        Util/DeviceUtil.cpp

        Test/DeviceSorterTest.cpp
        Algorithm/DeviceSort/DeviceSorter.hpp
        Algorithm/DeviceSort/DeviceSorter.cpp
        Algorithm/DeviceSort/DeviceSorter.slangh
        Algorithm/DeviceSort/DeviceSorterSizes.slangh
        Algorithm/DeviceSort/Reset.cs.slang
        Algorithm/DeviceSort/GlobalHist.cs.slang
        Algorithm/DeviceSort/ScanHist.cs.slang
        Algorithm/DeviceSort/OneSweep.cs.slang

        Test/IndirectDispatchTest.cpp

        Test/GS3DIndLightAlgoTest.cpp
        Algorithm/AliasTable.cpp
        Renderer/IndLight/GS3D/GS3DIndLightAlgo.hpp
        Renderer/IndLight/GS3D/GS3DIndLightAlgo.cpp
)
target_link_libraries(GaussianGITest PRIVATE args tbb)
target_copy_shaders(GaussianGITest GaussianGI)
target_source_group(GaussianGITest "GaussianGI")

add_falcor_executable(GaussianGITrain)
set_target_properties(GaussianGITrain PROPERTIES CXX_STANDARD 20)
target_sources(GaussianGITrain PRIVATE
        GaussianGITrain.hpp
        GaussianGITrain.cpp

        Util/DeviceUtil.hpp
        Util/DeviceUtil.cpp
        Util/SOABufferUtil.hpp
        Util/SOABufferDefine.slangh

        Scene/GMesh.hpp
        Scene/GMesh.cpp
        Scene/GMesh.slang
        Scene/GMesh.slangh
        Scene/GMeshLoader.hpp
        Scene/GMeshLoader.cpp
        Scene/GMeshView.hpp
        Scene/GMeshGSTrainDataset.3d.slang
        Scene/GMeshGSTrainDataset.hpp
        Scene/GMeshGSTrainDataset.cpp
        Scene/GMeshGSTrainSplatInit.cs.slang
        Scene/GMeshGSTrainSplatInit.hpp
        Scene/GMeshGSTrainSplatInit.cpp

        Algorithm/AliasTable.hpp
        Algorithm/AliasTable.cpp
        Algorithm/MeshView.hpp
        Algorithm/MeshSample.hpp
        Algorithm/MeshBVH.hpp
        Algorithm/MeshClosestPoint.hpp
        Algorithm/MeshGSOptimize.hpp
        Algorithm/ThreadGroupTilingX.slang
        Algorithm/ShaderMisc.slang
        Algorithm/GS3D.slang
        Algorithm/GS3DBound.hpp
        Algorithm/GS3DBound.slangh

        Algorithm/DeviceSort/DeviceSorter.hpp
        Algorithm/DeviceSort/DeviceSorter.cpp
        Algorithm/DeviceSort/DeviceSorter.slangh
        Algorithm/DeviceSort/DeviceSorterSizes.slangh
        Algorithm/DeviceSort/Reset.cs.slang
        Algorithm/DeviceSort/GlobalHist.cs.slang
        Algorithm/DeviceSort/ScanHist.cs.slang
        Algorithm/DeviceSort/OneSweep.cs.slang

        Algorithm/MeshGSTrainer/ForwardView.cs.slang
        Algorithm/MeshGSTrainer/ForwardDraw.3d.slang
        Algorithm/MeshGSTrainer/Loss.cs.slang
        Algorithm/MeshGSTrainer/BackwardDraw.3d.slang
        Algorithm/MeshGSTrainer/BackwardView.cs.slang
        Algorithm/MeshGSTrainer/BackwardCmd.cs.slang
        Algorithm/MeshGSTrainer/Optimize.cs.slang
        Algorithm/MeshGSTrainer/MeshGSTrainer.slang
        Algorithm/MeshGSTrainer/MeshGSTrainer.slangh
        Algorithm/MeshGSTrainer/MeshGSTrainer.hpp
        Algorithm/MeshGSTrainer/MeshGSTrainerImpl.inl
        Algorithm/MeshGSTrainer/Trait/ITrait.slangh
        Algorithm/MeshGSTrainer/Trait/Depth.slangh
        Algorithm/MeshGSTrainer/Trait/Depth.hpp
        Algorithm/MeshGSTrainer/Trait/Depth.cpp
        Algorithm/MeshGSTrainer/Trait/DepthAlbedo.slangh
        Algorithm/MeshGSTrainer/Trait/DepthAlbedo.hpp
        Algorithm/MeshGSTrainer/Trait/DepthAlbedo.cpp

        Renderer/IndLight/GS3D/GS3DIndLightSplat.hpp
        Renderer/IndLight/GS3D/GS3DIndLightSplatPersist.cpp
)
target_link_libraries(GaussianGITrain PRIVATE assimp tbb)
target_copy_shaders(GaussianGITrain GaussianGI)
target_source_group(GaussianGITrain "GaussianGI")
