add_falcor_executable(GaussianGI)

target_sources(GaussianGI PRIVATE
        GaussianGI.cpp
        GaussianGI.h

        Scene/GScene.cpp
        Scene/GScene.slang
        Scene/GScene.3d.slang
        Scene/GMesh.slangh
        Scene/GMeshLoader.cpp
        Scene/GTransform.slang
        Scene/GStaticScene.cpp
        Scene/GLighting.cpp
        Scene/GLighting.slang
)
target_link_libraries(GaussianGI PRIVATE assimp)

target_copy_shaders(GaussianGI GaussianGI)
target_source_group(GaussianGI "GaussianGI")
