add_falcor_executable(acsmp_physics_sandbox)


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:18388608")

target_sources(acsmp_physics_sandbox PRIVATE
    acsmp_physics_sandbox.cpp
    acsmp_physics_sandbox.h
    computeShader.cpp
    computeShader.h
	pixelShader.cpp
    pixelShader.h

    hlsl/render_Common.hlsli
    hlsl/PBR.hlsli
    hlsl/material.hlsli
    hlsl/compute_tonemapper.hlsl
)



target_copy_shaders(acsmp_physics_sandbox Samples/acsmp_physics_sandbox)

target_source_group(acsmp_physics_sandbox "Samples")


target_link_libraries(acsmp_physics_sandbox PUBLIC Dinput8.lib PUBLIC DXGuid.lib PUBLIC  Xinput9_1_0.lib)

target_include_directories(acsmp_physics_sandbox PUBLIC "nlohmann")
