#ifndef FUSION_SERVICE_H
#define FUSION_SERVICE_H

#include "../calibration/calibration.h"
#include "../vulkan/vk_context.h"
#include "../vulkan/vk_dispatch.h"
#include "../sync/timestamp_align.h"
#include "../hal/uvc_camera.h"
#include "../math/kalman.h"
#include "../input/button_input.h"
#include "../hud/hud_renderer.h"
#include "../distortion/lens_correction.h"
#include "stereo_depth.h"
#include "ir_overlay.h"

#define FUSION_IPC_SOCKET "/dev/socket/ir_fusion"

typedef struct {
    CalibrationData  cal;
    VKContext        vk;
    FusionDispatch   dispatch;
    TSRingBuffer     ir_ring;
    UVCCamera        ir_cam;
    KalmanState6     kalman;
    IROverlayState   overlay;
    StereoDepthParams depth_params;
    HUDState         hud;
    LensCoeffs       lens;
    ButtonInput      btn;
    int              btn_pipe[2];
    int              ipc_fd;
    volatile int     running;
} FusionService;

int  fusion_service_init(FusionService *svc, const char *cal_path,
                         int ir_vendor, int ir_product,
                         int ir_width, int ir_height, int ir_fps);
void fusion_service_run(FusionService *svc);
void fusion_service_shutdown(FusionService *svc);

#endif