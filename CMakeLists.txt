add_falcor_executable(GaussianGI)

target_sources(GaussianGI PRIVATE
        GaussianGI.cpp
        GaussianGI.h

        Scene/GScene.cpp
        Scene/GMeshLoader.cpp
)
target_link_libraries(GaussianGI PRIVATE assimp)

target_copy_shaders(GaussianGI GaussianGI)
target_source_group(GaussianGI "GaussianGI")
