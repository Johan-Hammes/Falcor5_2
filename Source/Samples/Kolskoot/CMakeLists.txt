add_falcor_executable(Kolskoot)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8388608")



target_sources(Kolskoot PRIVATE
    Kolskoot.cpp
    Kolskoot.h
    PointGrey_Camera.cpp
    PointGrey_Camera.h
    SerialComms.cpp
    SerialComms.h

    #FlyCapture/FlyCapture2.h
    #FlyCapture/Error.h
	
	#harupdf/include/hpdf.h
)



include_directories("${PROJECT_SOURCE_DIR}/FlyCapture")
include_directories("${PROJECT_SOURCE_DIR}/harupdf")
include_directories("${PROJECT_SOURCE_DIR}/harupdf/include")
target_link_libraries(Kolskoot PUBLIC winmm.lib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/FlyCapture/FlyCapture2_v140.lib" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/harupdf/hpdf.lib")
##target_link_libraries(Kolskoot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/FlyCapture/FlyCapture2d_v140.lib")




target_copy_shaders(Kolskoot Samples/Kolskoot)

target_source_group(Kolskoot "Samples")
