#ifndef UVC_CAMERA_H
#define UVC_CAMERA_H

#include <stdint.h>
#include <stddef.h>

typedef struct UVCFrame {
    uint8_t *data;
    size_t data_bytes;
    int width;
    int height;
    uint64_t timestamp_ns;
    int sequence;
} UVCFrame;

typedef void (*UVCFrameCallback)(const UVCFrame *frame, void *userdata);

typedef struct {
    void *ctx;
    void *dev;
    void *devh;
    void *ctrl;
    UVCFrameCallback frame_cb;
    void *userdata;
    int width;
    int height;
    int fps;
    int vendor_id;
    int product_id;
    volatile int running;
} UVCCamera;

int uvc_camera_open(UVCCamera *cam, int vendor_id, int product_id,
                    int width, int height, int fps,
                    UVCFrameCallback cb, void *userdata);
void uvc_camera_close(UVCCamera *cam);
int uvc_camera_get_frame_blocking(UVCCamera *cam, UVCFrame *out, int timeout_ms);

#endif