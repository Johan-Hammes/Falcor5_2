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
	
	compute_tileVertices.hlsl
	compute_bc6h_functions.hlsli
	gpuLights_defines.hlsli
	gpuLights_functions.hlsli
	groundcover_defines.hlsli
	groundcover_functions.hlsli
	terrainDefines.hlsli
	terrainFunctions.hlsli
	compute_bc6h.hlsl
	compute_tileBicubic.hlsl
	compute_tileBuildLookup.hlsl
	compute_tileClear.hlsl
	compute_tileDelaunay.hlsl
	compute_tileEcotopes.hlsl
	compute_tileElevationMipmap.hlsl
	compute_tileGenerate.hlsl
	compute_tileJumpFlood.hlsl
	compute_tileNormals.hlsl
	compute_tilePassthrough.hlsl
	compute_tileSplitMerge.hlsl 
)

target_copy_shaders(Earthworks_4 Samples/Earthworks_4)

target_source_group(Earthworks_4 "Samples")