add_falcor_executable(Earthworks_4)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:18388608")

target_sources(Earthworks_4 PRIVATE
    Earthworks_4.cpp
    Earthworks_4.h
    computeShader.cpp
    computeShader.h
    ecotope.h
    ecotope.cpp
    roads_AI.cpp
    roads_AI.h
    roads_bezier.cpp
    roads_bezier.h
    roads_Intersection.cpp
    roads_Intersection.h
    roads_materials.cpp
    roads_materials.h
    roads_physics.cpp
    roads_physics.h
    roads_road.cpp
    roads_road.h
    roadNetwork.cpp
    roadNetwork.h
    Sprites.h
    Sprites.cpp
    Sprite_defines.h
    terrafector.cpp
    terrafector.h
	terrain.cpp
    terrain.h
	pixelShader.cpp
    pixelShader.h
	
	hlsl/compute_tileVertices.hlsl
	hlsl/compute_bc6h_functions.hlsli
	hlsl/gpuLights_defines.hlsli
	hlsl/gpuLights_functions.hlsli
	hlsl/groundcover_defines.hlsli
	hlsl/groundcover_functions.hlsli
    hlsl/materials.hlsli
	hlsl/terrainDefines.hlsli
	hlsl/terrainFunctions.hlsli
	hlsl/compute_bc6h.hlsl
	hlsl/compute_tileBicubic.hlsl
	hlsl/compute_tileBuildLookup.hlsl
	hlsl/compute_tileClear.hlsl
	hlsl/compute_tileDelaunay.hlsl
	hlsl/compute_tileEcotopes.hlsl
	hlsl/compute_tileGenerate.hlsl
	hlsl/compute_tileJumpFlood.hlsl
	hlsl/compute_tileNormals.hlsl
	hlsl/compute_tilePassthrough.hlsl
	hlsl/compute_tileSplitMerge.hlsl
    hlsl/compute_terrain_under_mouse.hlsl

    hlsl/render_meshTerrafector.hlsl
    hlsl/render_splineTerrafector.hlsl
    hlsl/render_spline.hlsl
    hlsl/render_sprite.hlsl
    hlsl/render_tile_sprite.hlsl
    hlsl/render_ribbons.hlsl
    hlsl/render_Tiles.hlsl
    hlsl/render_Common.hlsli
    hlsl/PBR.hlsli
    hlsl/material.hlsli
)



target_copy_shaders(Earthworks_4 Samples/Earthworks_4)

target_source_group(Earthworks_4 "Samples")


