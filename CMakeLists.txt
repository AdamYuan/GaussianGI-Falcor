add_falcor_executable(GaussianGI)

target_sources(GaussianGI PRIVATE
        GaussianGI.cpp
        GaussianGI.h

        Scene/GBound.hpp
        Scene/GScene.hpp
        Scene/GScene.cpp
        Scene/GScene.slang
        Scene/GScene.3d.slang
        Scene/GMesh.hpp
        Scene/GMesh.slangh
        Scene/GMeshLoader.hpp
        Scene/GMeshLoader.cpp
        Scene/GTransform.hpp
        Scene/GTransform.slang
        Scene/GStaticScene.hpp
        Scene/GStaticScene.cpp
        Scene/GLighting.hpp
        Scene/GLighting.cpp
        Scene/GLighting.slang
        Scene/GSceneObject.hpp
)
target_link_libraries(GaussianGI PRIVATE assimp)

target_copy_shaders(GaussianGI GaussianGI)
target_source_group(GaussianGI "GaussianGI")
