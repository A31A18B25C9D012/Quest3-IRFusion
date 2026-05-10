add_library(ir_sync STATIC
    timestamp_align.c
)
target_include_directories(ir_sync PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ir_sync pthread)
target_compile_options(ir_sync PRIVATE -Wall -O2)