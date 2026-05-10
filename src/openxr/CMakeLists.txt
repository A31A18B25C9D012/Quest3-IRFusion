find_path(OPENXR_INCLUDE_DIR openxr/openxr.h REQUIRED)
find_library(OPENXR_LOADER openxr_loader REQUIRED)

add_library(ir_openxr STATIC
    compositor.c
)
target_include_directories(ir_openxr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${OPENXR_INCLUDE_DIR})
target_link_libraries(ir_openxr ${OPENXR_LOADER} ir_vulkan)
target_compile_options(ir_openxr PRIVATE -Wall -O2)