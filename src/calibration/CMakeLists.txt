add_library(ir_calibration STATIC
    calibration.c
)
target_include_directories(ir_calibration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ir_calibration ir_math m)
target_compile_options(ir_calibration PRIVATE -Wall -O2)