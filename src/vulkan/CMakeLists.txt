find_package(Vulkan REQUIRED)

add_library(ir_vulkan STATIC
    vk_context.c
    vk_image.c
    vk_pipeline.c
    vk_dispatch.c
)
target_include_directories(ir_vulkan PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ir_vulkan Vulkan::Vulkan)

if(ANDROID)
    target_compile_definitions(ir_vulkan PRIVATE __ANDROID__)
    target_link_libraries(ir_vulkan android log)
endif()
target_compile_options(ir_vulkan PRIVATE -Wall -O2)