add_falcor_executable(GaussianGI)
set_target_properties(GaussianGI PROPERTIES CXX_STANDARD 20)
target_sources(GaussianGI PRIVATE
        GaussianGI.cpp
        GaussianGI.h

        Common/GDeviceObject.hpp

        Util/DeviceUtil.hpp
        Util/DeviceUtil.cpp
        Util/WaveOps.cs.slang
        Util/ShaderUtil.hpp
        Util/ShaderUtil.slang
        Util/TextureUtil.hpp
        Util/EnumUtil.hpp

        Algorithm/AliasTable.hpp
        Algorithm/AliasTable.cpp
        Algorithm/MeshView.hpp
        Algorithm/MeshSample.hpp
        Algorithm/MeshBVH.hpp
        Algorithm/MeshClosestPoint.hpp

        Algorithm/GPUSorting/DeviceSorter.hpp
        Algorithm/GPUSorting/DeviceSorter.cpp
        Algorithm/GPUSorting/OneSweep.cs.slang
        Algorithm/GPUSorting/SweepCommon.slangh
        Algorithm/GPUSorting/SortCommon.slangh

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

        Renderer/IndLight/GIndLight.hpp
        Renderer/IndLight/GIndLight.cpp
        Renderer/IndLight/GIndLightArgs.hpp
        Renderer/IndLight/GIndLightType.hpp
        Renderer/IndLight/NoIndLight.hpp

        Renderer/IndLight/PathTraced/PTIndLight.hpp
        Renderer/IndLight/PathTraced/PTIndLight.cpp
        Renderer/IndLight/PathTraced/PTIndLight.cs.slang

        Renderer/IndLight/3DGS/GS3D.hpp
        Renderer/IndLight/3DGS/GS3D.slang
        Renderer/IndLight/3DGS/GS3DIndLight.hpp
        Renderer/IndLight/3DGS/GS3DIndLight.cpp
        Renderer/IndLight/3DGS/GS3DIndLight.cs.slang
        Renderer/IndLight/3DGS/GS3DMiscRenderer.hpp
        Renderer/IndLight/3DGS/GS3DMiscRenderer.cpp
        Renderer/IndLight/3DGS/GS3DMiscPoint.3d.slang
        Renderer/IndLight/3DGS/GS3DMiscSplatView.cs.slang
        Renderer/IndLight/3DGS/GS3DMiscSplatDraw.3d.slang
)
target_link_libraries(GaussianGI PRIVATE assimp)
target_copy_shaders(GaussianGI GaussianGI)
target_source_group(GaussianGI "GaussianGI")

add_falcor_executable(GaussianGITest)
set_target_properties(GaussianGITest PROPERTIES CXX_STANDARD 20)
target_sources(GaussianGITest PRIVATE
        GaussianGITest.cpp

        Util/DeviceUtil.hpp
        Util/DeviceUtil.cpp
        Util/WaveOps.cs.slang

        Test/DeviceSorterTest.cpp
        Algorithm/GPUSorting/DeviceSorter.hpp
        Algorithm/GPUSorting/DeviceSorter.cpp
        Algorithm/GPUSorting/OneSweep.cs.slang
        Algorithm/GPUSorting/SweepCommon.slangh
        Algorithm/GPUSorting/SortCommon.slangh

        Test/IndirectDispatchTest.cpp
)
target_link_libraries(GaussianGITest PRIVATE args)
target_copy_shaders(GaussianGITest GaussianGI)
target_source_group(GaussianGITest "GaussianGI")
