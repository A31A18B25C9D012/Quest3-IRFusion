#include "camera_hal3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef __ANDROID__
#include <hardware/hardware.h>
#include <hardware/camera3.h>
#include <android/hardware_buffer.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraMetadata.h>

typedef struct {
    ACameraManager *manager;
    ACameraDevice *device;
    ACaptureSessionOutputContainer *container;
    ACaptureSessionOutput *output;
    ACameraCaptureSession *session;
    AImageReader *image_reader;
    ACaptureRequest *request;
    ANativeWindow *window;
    HAL3ResultCallback result_cb;
    void *userdata;
    int frame_number;
} AndroidHAL3Priv;

static void on_device_disconnected(void *ctx, ACameraDevice *device) {
    (void)ctx; (void)device;
}

static void on_device_error(void *ctx, ACameraDevice *device, int error) {
    (void)ctx; (void)device; (void)error;
}

static void on_session_active(void *ctx, ACameraCaptureSession *session) {
    (void)ctx; (void)session;
}

static void on_session_closed(void *ctx, ACameraCaptureSession *session) {
    (void)ctx; (void)session;
}

static void on_session_ready(void *ctx, ACameraCaptureSession *session) {
    (void)ctx; (void)session;
}

static void on_image_available(void *ctx, AImageReader *reader) {
    HAL3Context *hal = (HAL3Context *)ctx;
    AndroidHAL3Priv *priv = (AndroidHAL3Priv *)hal->device;
    AImage *image = NULL;
    if (AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK)
        return;
    int64_t timestamp = 0;
    AImage_getTimestamp(image, &timestamp);
    HAL3CaptureResult result;
    memset(&result, 0, sizeof(result));
    result.buffer_handle = image;
    result.timestamp_ns = (uint64_t)timestamp;
    result.stream_index = 0;
    result.status = 0;
    if (priv->result_cb)
        priv->result_cb(&result, priv->userdata);
    AImage_delete(image);
}

int hal3_open(HAL3Context *ctx, const char *camera_id) {
    AndroidHAL3Priv *priv = (AndroidHAL3Priv *)calloc(1, sizeof(AndroidHAL3Priv));
    if (!priv) return -ENOMEM;
    ctx->device = priv;
    priv->manager = ACameraManager_create();
    if (!priv->manager) { free(priv); return -1; }
    ACameraDevice_StateCallbacks dcb;
    dcb.context = ctx;
    dcb.onDisconnected = on_device_disconnected;
    dcb.onError = on_device_error;
    camera_status_t st = ACameraManager_openCamera(priv->manager, camera_id, &dcb, &priv->device);
    if (st != ACAMERA_OK) {
        ACameraManager_delete(priv->manager);
        free(priv);
        ctx->device = NULL;
        return -1;
    }
    return 0;
}

int hal3_configure_streams(HAL3Context *ctx, HAL3StreamConfig *configs, int count) {
    AndroidHAL3Priv *priv = (AndroidHAL3Priv *)ctx->device;
    if (!priv || count < 1) return -1;
    media_status_t ms = AImageReader_new(configs[0].width, configs[0].height,
                                         AIMAGE_FORMAT_YUV_420_888, 4, &priv->image_reader);
    if (ms != AMEDIA_OK) return -1;
    AImageReader_ImageListener listener;
    listener.context = ctx;
    listener.onImageAvailable = on_image_available;
    AImageReader_setImageListener(priv->image_reader, &listener);
    AImageReader_getWindow(priv->image_reader, &priv->window);
    ACaptureSessionOutputContainer_create(&priv->container);
    ACaptureSessionOutput_create(priv->window, &priv->output);
    ACaptureSessionOutputContainer_add(priv->container, priv->output);
    memcpy(ctx->streams, configs, count * sizeof(HAL3StreamConfig));
    ctx->stream_count = count;
    return 0;
}

int hal3_start_capture(HAL3Context *ctx, HAL3ResultCallback cb, void *userdata) {
    AndroidHAL3Priv *priv = (AndroidHAL3Priv *)ctx->device;
    if (!priv) return -1;
    priv->result_cb = cb;
    priv->userdata = userdata;
    ACameraCaptureSession_stateCallbacks scb;
    scb.context = ctx;
    scb.onActive = on_session_active;
    scb.onClosed = on_session_closed;
    scb.onReady = on_session_ready;
    camera_status_t st = ACameraDevice_createCaptureSession(priv->device, priv->container, &scb, &priv->session);
    if (st != ACAMERA_OK) return -1;
    ACameraDevice_createCaptureRequest(priv->device, TEMPLATE_PREVIEW, &priv->request);
    ACameraOutputTarget *target = NULL;
    ACameraOutputTarget_create(priv->window, &target);
    ACaptureRequest_addTarget(priv->request, target);
    ACameraOutputTarget_free(target);
    ACameraCaptureSession_setRepeatingRequest(priv->session, NULL, 1, &priv->request, NULL);
    return 0;
}

void hal3_stop_capture(HAL3Context *ctx) {
    AndroidHAL3Priv *priv = (AndroidHAL3Priv *)ctx->device;
    if (!priv || !priv->session) return;
    ACameraCaptureSession_stopRepeating(priv->session);
    ACameraCaptureSession_close(priv->session);
    priv->session = NULL;
}

void hal3_close(HAL3Context *ctx) {
    AndroidHAL3Priv *priv = (AndroidHAL3Priv *)ctx->device;
    if (!priv) return;
    if (priv->request) ACaptureRequest_free(priv->request);
    if (priv->output) ACaptureSessionOutput_free(priv->output);
    if (priv->container) ACaptureSessionOutputContainer_free(priv->container);
    if (priv->image_reader) AImageReader_delete(priv->image_reader);
    if (priv->device) ACameraDevice_close(priv->device);
    if (priv->manager) ACameraManager_delete(priv->manager);
    free(priv);
    ctx->device = NULL;
}

int hal3_submit_request(HAL3Context *ctx, int stream_mask) {
    (void)stream_mask;
    AndroidHAL3Priv *priv = (AndroidHAL3Priv *)ctx->device;
    if (!priv || !priv->session) return -1;
    return 0;
}

#else

int hal3_open(HAL3Context *ctx, const char *camera_id) {
    (void)camera_id;
    ctx->device = NULL;
    return -1;
}
int hal3_configure_streams(HAL3Context *ctx, HAL3StreamConfig *configs, int count) {
    (void)ctx; (void)configs; (void)count; return -1;
}
int hal3_start_capture(HAL3Context *ctx, HAL3ResultCallback cb, void *userdata) {
    (void)ctx; (void)cb; (void)userdata; return -1;
}
void hal3_stop_capture(HAL3Context *ctx) { (void)ctx; }
void hal3_close(HAL3Context *ctx) { (void)ctx; }
int hal3_submit_request(HAL3Context *ctx, int stream_mask) {
    (void)ctx; (void)stream_mask; return -1;
}
#endif