add_falcor_executable(Earthworks_4)

target_sources(Earthworks_4 PRIVATE
    Earthworks_4.cpp
    Earthworks_4.h
	terrain.cpp
    terrain.h
	computeShader.cpp
    computeShader.h
	pixelShader.cpp
    pixelShader.h
	
	add_subdirectory(cereal)
)

target_copy_shaders(Earthworks_4 Samples/Earthworks_4)

target_source_group(Earthworks_4 "Samples")
