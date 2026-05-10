find_library(LIBUVC uvc REQUIRED)
find_library(LIBUSB usb-1.0 REQUIRED)

add_library(ir_hal STATIC
    camera_hal3.c
    uvc_camera.c
)
target_include_directories(ir_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ir_hal ${LIBUVC} ${LIBUSB})

if(ANDROID)
    target_link_libraries(ir_hal camera2ndk mediandk)
    target_compile_definitions(ir_hal PRIVATE __ANDROID__)
endif()
target_compile_options(ir_hal PRIVATE -Wall -O2)