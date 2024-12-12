add_falcor_executable(GaussianGI)
set_target_properties(GaussianGI PROPERTIES CXX_STANDARD 20)

target_sources(GaussianGI PRIVATE
        GaussianGI.cpp
        GaussianGI.h

        Common/GDeviceObject.hpp
        Common/ShaderUtil.hpp
        Common/ShaderUtil.slang
        Common/TextureUtil.hpp
        Common/EnumUtil.hpp

        Scene/GBound.hpp
        Scene/GScene.hpp
        Scene/GScene.cpp
        Scene/GScene.slang
        Scene/GScene.3d.slang
        Scene/GMesh.hpp
        Scene/GMesh.cpp
        Scene/GMesh.slang
        Scene/GMesh.slangh
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
        Renderer/GVBuffer.hpp
        Renderer/GVBuffer.cpp
        Renderer/GVBuffer.slang
        Renderer/GVBuffer.3d.slang

        Renderer/Shadow/GShadow.hpp
        Renderer/Shadow/GShadow.cpp
        Renderer/Shadow/GShadow.slang
        Renderer/Shadow/GShadowType.hpp

        Renderer/Shadow/RT/RTShadow.slang

        Renderer/Shadow/PCF/PCFShadow.hpp
        Renderer/Shadow/PCF/PCFShadow.cpp

        Renderer/Shadow/EVSM/EVSMShadow.hpp
        Renderer/Shadow/EVSM/EVSMShadow.cpp
)
target_link_libraries(GaussianGI PRIVATE assimp)

target_copy_shaders(GaussianGI GaussianGI)
target_source_group(GaussianGI "GaussianGI")
