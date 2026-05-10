#include "uvc_camera.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <libuvc/libuvc.h>

static uint64_t timespec_to_ns(struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000000000ULL + (uint64_t)ts->tv_nsec;
}

static void internal_frame_cb(uvc_frame_t *frame, void *user) {
    UVCCamera *cam = (UVCCamera *)user;
    if (!cam->running) return;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    UVCFrame out;
    out.data = (uint8_t *)frame->data;
    out.data_bytes = frame->data_bytes;
    out.width = (int)frame->width;
    out.height = (int)frame->height;
    out.timestamp_ns = timespec_to_ns(&ts);
    out.sequence = (int)frame->sequence;
    if (cam->frame_cb)
        cam->frame_cb(&out, cam->userdata);
}

int uvc_camera_open(UVCCamera *cam, int vendor_id, int product_id,
                    int width, int height, int fps,
                    UVCFrameCallback cb, void *userdata) {
    uvc_context_t *ctx = NULL;
    uvc_device_t *dev = NULL;
    uvc_device_handle_t *devh = NULL;
    uvc_stream_ctrl_t *ctrl = NULL;
    uvc_error_t res;

    res = uvc_init(&ctx, NULL);
    if (res != UVC_SUCCESS) return -1;

    res = uvc_find_device(ctx, &dev, vendor_id, product_id, NULL);
    if (res != UVC_SUCCESS) {
        uvc_exit(ctx);
        return -1;
    }

    res = uvc_open(dev, &devh);
    if (res != UVC_SUCCESS) {
        uvc_unref_device(dev);
        uvc_exit(ctx);
        return -1;
    }

    ctrl = (uvc_stream_ctrl_t *)calloc(1, sizeof(uvc_stream_ctrl_t));
    if (!ctrl) {
        uvc_close(devh);
        uvc_unref_device(dev);
        uvc_exit(ctx);
        return -1;
    }

    res = uvc_get_stream_ctrl_format_size(devh, ctrl, UVC_FRAME_FORMAT_GRAY8, width, height, fps);
    if (res != UVC_SUCCESS) {
        res = uvc_get_stream_ctrl_format_size(devh, ctrl, UVC_FRAME_FORMAT_YUYV, width, height, fps);
        if (res != UVC_SUCCESS) {
            free(ctrl);
            uvc_close(devh);
            uvc_unref_device(dev);
            uvc_exit(ctx);
            return -1;
        }
    }

    cam->ctx = ctx;
    cam->dev = dev;
    cam->devh = devh;
    cam->ctrl = ctrl;
    cam->frame_cb = cb;
    cam->userdata = userdata;
    cam->width = width;
    cam->height = height;
    cam->fps = fps;
    cam->vendor_id = vendor_id;
    cam->product_id = product_id;
    cam->running = 1;

    res = uvc_start_streaming(devh, ctrl, internal_frame_cb, cam, 0);
    if (res != UVC_SUCCESS) {
        cam->running = 0;
        free(ctrl);
        uvc_close(devh);
        uvc_unref_device(dev);
        uvc_exit(ctx);
        return -1;
    }

    return 0;
}

void uvc_camera_close(UVCCamera *cam) {
    cam->running = 0;
    if (cam->devh) {
        uvc_stop_streaming((uvc_device_handle_t *)cam->devh);
        uvc_close((uvc_device_handle_t *)cam->devh);
    }
    if (cam->dev)
        uvc_unref_device((uvc_device_t *)cam->dev);
    if (cam->ctx)
        uvc_exit((uvc_context_t *)cam->ctx);
    if (cam->ctrl)
        free(cam->ctrl);
    cam->ctx = cam->dev = cam->devh = cam->ctrl = NULL;
}

int uvc_camera_get_frame_blocking(UVCCamera *cam, UVCFrame *out, int timeout_ms) {
    (void)cam; (void)out; (void)timeout_ms;
    return -1;
}