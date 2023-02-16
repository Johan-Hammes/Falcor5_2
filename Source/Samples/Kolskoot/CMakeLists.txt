add_falcor_executable(Kolskoot)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8388608")

#add_library( flycap SHARED IMPORTED )
#target_include_directories( flycap INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/FlyCapture" )
#target_link_libraries( flycap INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/FlyCapture/FlyCapture2_v100.lib" )
#target_link_libraries( flycap INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/FlyCapture/FlyCapture2_v100.lib" )


add_library(flycap SHARED IMPORTED)
set_target_properties(flycap PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}Flycapture/FlyCapture2_v100.dll"
    IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}Flycapture/FlyCapture2_v100.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/Flycapture"
)
target_link_libraries(flycap)



target_sources(Kolskoot PRIVATE
    Kolskoot.cpp
    Kolskoot.h
    PointGrey_Camera.cpp
    PointGrey_Camera.h

    #FlyCapture/FlyCapture2.h
    #FlyCapture/Error.h
)

# flycap
#add_library(flycap INTERFACE)
#target_link_libraries(flycap INTERFACE FlyCapture2_v100.lib)

#add_library(flycap SHARED IMPORTED)
#set_target_properties(flycap PROPERTIES
#    IMPORTED_LOCATION "Flycapture/FlyCapture2_v100.dll"
#    IMPORTED_IMPLIB "Flycapture/FlyCapture2_v100.lib"
#    INTERFACE_INCLUDE_DIRECTORIES "Flycapture"
#)
#target_link_libraries(flycap INTERFACE FlyCapture2_v100.lib)





target_copy_shaders(Kolskoot Samples/Kolskoot)

target_source_group(Kolskoot "Samples")
