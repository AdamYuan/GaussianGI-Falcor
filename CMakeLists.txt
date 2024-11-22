add_falcor_executable(GaussianGI)

target_sources(GaussianGI PRIVATE
    GaussianGI.cpp
    GaussianGI.h
)

target_copy_shaders(GaussianGI GaussianGI/GaussianGI)

target_source_group(GaussianGI "GaussianGI")
