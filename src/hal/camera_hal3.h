#ifndef CAMERA_HAL3_H
#define CAMERA_HAL3_H

#include <stdint.h>
#include <stddef.h>

#ifdef __ANDROID__
#include <hardware/camera3.h>
#include <android/hardware_buffer.h>
#endif

#define HAL3_MAX_STREAMS 4

typedef struct {
    int width;
    int height;
    int format;
    int usage;
} HAL3StreamConfig;

typedef struct {
    void *buffer_handle;
    uint64_t timestamp_ns;
    int stream_index;
    int status;
} HAL3CaptureResult;

typedef void (*HAL3ResultCallback)(const HAL3CaptureResult *result, void *userdata);

typedef struct {
    void *device;
    void *provider;
    int stream_count;
    HAL3StreamConfig streams[HAL3_MAX_STREAMS];
    HAL3ResultCallback result_cb;
    void *userdata;
    int frame_number;
} HAL3Context;

int hal3_open(HAL3Context *ctx, const char *camera_id);
int hal3_configure_streams(HAL3Context *ctx, HAL3StreamConfig *configs, int count);
int hal3_start_capture(HAL3Context *ctx, HAL3ResultCallback cb, void *userdata);
void hal3_stop_capture(HAL3Context *ctx);
void hal3_close(HAL3Context *ctx);
int hal3_submit_request(HAL3Context *ctx, int stream_mask);

#endif