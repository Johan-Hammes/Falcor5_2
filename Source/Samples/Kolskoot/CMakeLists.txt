add_falcor_executable(Kolskoot)

target_sources(Kolskoot PRIVATE
    Kolskoot.cpp
    Kolskoot.h
)

target_copy_shaders(Kolskoot Samples/Kolskoot)

target_source_group(Kolskoot "Samples")
