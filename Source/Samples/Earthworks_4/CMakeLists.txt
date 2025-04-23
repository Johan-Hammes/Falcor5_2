add_falcor_executable(Earthworks_4)


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:18388608")

target_sources(Earthworks_4 PRIVATE
    Earthworks_4.cpp
    Earthworks_4.h
    atmosphere.cpp
    atmosphere.h
    computeShader.cpp
    computeShader.h
    Sprites.h
    Sprites.cpp
    Sprite_defines.h
	pixelShader.cpp
    pixelShader.h
    volumeFogPhaseFunctions.cpp
    Barrier.hpp

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
    terrafector.cpp
    terrafector.h
	terrain.cpp
    terrain.h
    glider.cpp
    glider_runtime.cpp
    glider.h
    cfd.h
    cfd.cpp
	vegetationBuilder.h
    vegetationBuilder.cpp


	hlsl/terrain/compute_tileVertices.hlsl
	hlsl/terrain/compute_bc6h_functions.hlsli
	hlsl/terrain/gpuLights_defines.hlsli
	hlsl/terrain/gpuLights_functions.hlsli
	hlsl/terrain/groundcover_defines.hlsli
	hlsl/terrain/groundcover_functions.hlsli
    hlsl/terrain/materials.hlsli
	hlsl/terrain/terrainDefines.hlsli
	hlsl/terrain/terrainFunctions.hlsli
	hlsl/terrain/compute_bc6h.hlsl
	hlsl/terrain/compute_tileBicubic.hlsl
	hlsl/terrain/compute_tileBuildLookup.hlsl
	hlsl/terrain/compute_tileClear.hlsl
	hlsl/terrain/compute_tileDelaunay.hlsl
	hlsl/terrain/compute_tileEcotopes.hlsl
	hlsl/terrain/compute_tileGenerate.hlsl
	hlsl/terrain/compute_tileJumpFlood.hlsl
	hlsl/terrain/compute_tileNormals.hlsl
	hlsl/terrain/compute_tilePassthrough.hlsl
	hlsl/terrain/compute_tileSplitMerge.hlsl
    hlsl/terrain/compute_terrain_under_mouse.hlsl
    hlsl/terrain/compute_clipLodAnimatePlants.hlsl

    hlsl/terrain/compute_bakeFloodfill.hlsl

    hlsl/terrain/render_meshTerrafector.hlsl
    hlsl/terrain/render_ribbons.hlsl
    hlsl/terrain/vegetation_defines.hlsli
    hlsl/terrain/render_vegetation_ribbons.hlsl
    hlsl/terrain/render_thermalRibbons.hlsl
    hlsl/terrain/render_cfdSlice.hlsl
    hlsl/terrain/render_splineTerrafector.hlsl
    hlsl/terrain/render_spline.hlsl
    hlsl/terrain/render_tile_sprite.hlsl
    hlsl/terrain/render_Tiles.hlsl
    hlsl/terrain/render_triangles.hlsl
    hlsl/terrain/render_Buildings_Far.hlsl
    hlsl/terrain/render_GliderWing.hlsl

    hlsl/atmosphere/compute_fogCloudAtmosphereCommon.hlsli
    hlsl/atmosphere/compute_volumeFog.hlsli
    hlsl/atmosphere/compute_sunlightInAtmosphere.hlsl
    hlsl/atmosphere/compute_volumeFogAtmosphericScatter.hlsl
    hlsl/atmosphere/compute_volumeFogLights.hlsl
    hlsl/atmosphere/compute_volumeFogSmokeAndDust.hlsl

    
    hlsl/render_sprite.hlsl
    hlsl/render_Common.hlsli
    hlsl/PBR.hlsli
    hlsl/material.hlsli
    hlsl/compute_tonemapper.hlsl
)



target_copy_shaders(Earthworks_4 Samples/Earthworks_4)

target_source_group(Earthworks_4 "Samples")


target_link_libraries(Earthworks_4 PUBLIC Dinput8.lib PUBLIC DXGuid.lib PUBLIC  Xinput9_1_0.lib)

target_include_directories(Earthworks_4 PUBLIC "nlohmann")
