cmake_minimum_required(VERSION 3.22)
project(IRFusion C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a")
endif()

find_package(Vulkan REQUIRED)

set(IR_FUSION_COMPILE_OPTIONS
    -Wall -Wextra -O2
    -march=armv8-a+simd
    -mfpu=neon-fp-armv8
)

add_subdirectory(src/math)
add_subdirectory(src/sync)
add_subdirectory(src/hal)
add_subdirectory(src/calibration)
add_subdirectory(src/vulkan)
add_subdirectory(src/service)
add_subdirectory(src/openxr)

add_executable(ir_fusion_service
    src/service/fusion_service.c
    src/service/stereo_depth.c
    src/service/ir_overlay.c
)

target_link_libraries(ir_fusion_service
    ir_math
    ir_sync
    ir_hal
    ir_calibration
    ir_vulkan
    ir_openxr
    pthread
    m
)

target_compile_options(ir_fusion_service PRIVATE ${IR_FUSION_COMPILE_OPTIONS})

find_package(OpenCV REQUIRED COMPONENTS core imgproc calib3d highgui)
add_executable(calibrate
    src/calibration/calibrate.c
    src/calibration/calibration.c
    src/math/camera_math.c
    src/math/se3.c
    src/math/mat4.c
)
target_link_libraries(calibrate ${OpenCV_LIBS} m)
target_compile_options(calibrate PRIVATE -Wall -O2)

find_program(GLSLC glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)
if(GLSLC)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/ir_warp.spv
        COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/src/vulkan/shaders/ir_warp.comp -o ${CMAKE_BINARY_DIR}/ir_warp.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/src/vulkan/shaders/ir_warp.comp
    )
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/composite.spv
        COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/src/vulkan/shaders/composite.comp -o ${CMAKE_BINARY_DIR}/composite.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/src/vulkan/shaders/composite.comp
    )
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/edge_detect.spv
        COMMAND ${GLSLC} ${CMAKE_SOURCE_DIR}/src/vulkan/shaders/edge_detect.comp -o ${CMAKE_BINARY_DIR}/edge_detect.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/src/vulkan/shaders/edge_detect.comp
    )
    add_custom_target(shaders ALL DEPENDS
        ${CMAKE_BINARY_DIR}/ir_warp.spv
        ${CMAKE_BINARY_DIR}/composite.spv
        ${CMAKE_BINARY_DIR}/edge_detect.spv
    )
    add_dependencies(ir_fusion_service shaders)
endif()