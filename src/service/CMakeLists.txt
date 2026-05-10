add_library(ir_service STATIC
    stereo_depth.c
    ir_overlay.c
)
target_include_directories(ir_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ir_service ir_math)
target_compile_options(ir_service PRIVATE -Wall -O2 -march=armv8-a+simd)