add_library(ir_math STATIC
    mat4.c
    se3.c
    kalman.c
    camera_math.c
)
target_include_directories(ir_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(ir_math PRIVATE -Wall -O2 -march=armv8-a+simd)