#include "fusion_service.h"
#include "../math/se3.h"
#include "../math/camera_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#define IPC_BUF_SIZE    64
#define DEPTH_WIN_SIZE  9
#define DEPTH_MAX_DISP  64

extern const uint32_t ir_warp_spirv[];
extern const size_t   ir_warp_spirv_size;
extern const uint32_t composite_spirv[];
extern const size_t   composite_spirv_size;
extern const uint32_t edge_spirv[];
extern const size_t   edge_spirv_size;
extern const uint32_t hud_spirv[];
extern const size_t   hud_spirv_size;
extern const uint32_t lens_spirv[];
extern const size_t   lens_spirv_size;

static void ir_frame_callback(const UVCFrame *frame, void *userdata) {
    FusionService *svc = (FusionService *)userdata;
    if (!svc->running) return;
    ts_ring_push(&svc->ir_ring, frame->data, frame->timestamp_ns);
}

static void button_event_callback(ButtonEvent event, void *userdata) {
    FusionService *svc = (FusionService *)userdata;
    char msg;
    switch (event) {
    case BUTTON_VOLUME_UP:   msg = 'U'; break;
    case BUTTON_VOLUME_DOWN: msg = 'D'; break;
    case BUTTON_POWER_SHORT: msg = 'S'; break;
    case BUTTON_POWER_HOLD:  msg = 'H'; break;
    default: return;
    }
    write(svc->btn_pipe[1], &msg, 1);
}

static const int RES_W[3] = { 320,  640, 1280 };
static const int RES_H[3] = { 240,  480,  720 };

static void fusion_service_reinit_camera(FusionService *svc) {
    int preset  = svc->hud.ir_res_preset;
    int vendor  = svc->ir_cam.vendor_id;
    int product = svc->ir_cam.product_id;
    int fps     = svc->ir_cam.fps;
    int w       = RES_W[preset];
    int h       = RES_H[preset];
    uvc_camera_close(&svc->ir_cam);
    ts_ring_destroy(&svc->ir_ring);
    if (ts_ring_init(&svc->ir_ring, w, h, 1) != 0) return;
    uvc_camera_open(&svc->ir_cam, vendor, product, w, h, fps,
                    ir_frame_callback, svc);
}

static void settings_activate(FusionService *svc) {
    switch (svc->hud.settings_cursor) {
    case 0:
        ir_overlay_set_mode(&svc->overlay,
                            (IROverlayMode)((svc->overlay.mode + 1) % 5));
        hud_state_set_mode(&svc->hud, svc->overlay.mode);
        break;
    case 1:
        svc->hud.ir_res_preset = (svc->hud.ir_res_preset + 1) % 3;
        fusion_service_reinit_camera(svc);
        break;
    case 2:
        svc->hud.hud_enabled ^= 1;
        break;
    case 3:
        svc->hud.settings_open = 0;
        break;
    }
}

static void process_button_pipe(FusionService *svc) {
    char msg;
    ssize_t n;
    while ((n = read(svc->btn_pipe[0], &msg, 1)) == 1) {
        int cur = svc->overlay.mode;
        switch (msg) {
        case 'U':
            if (svc->hud.settings_open) {
                svc->hud.settings_cursor = (svc->hud.settings_cursor + 3) % 4;
            } else {
                ir_overlay_set_mode(&svc->overlay, (IROverlayMode)((cur + 1) % 5));
                hud_state_set_mode(&svc->hud, svc->overlay.mode);
            }
            break;
        case 'D':
            if (svc->hud.settings_open) {
                svc->hud.settings_cursor = (svc->hud.settings_cursor + 1) % 4;
            } else {
                ir_overlay_set_mode(&svc->overlay, (IROverlayMode)((cur + 4) % 5));
                hud_state_set_mode(&svc->hud, svc->overlay.mode);
            }
            break;
        case 'S':
            if (svc->hud.settings_open) {
                settings_activate(svc);
            } else {
                svc->hud.settings_open   = 1;
                svc->hud.settings_cursor = 0;
            }
            break;
        case 'H':
            svc->running = 0;
            break;
        default:
            break;
        }
    }
}

static int ipc_server_open(void) {
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, FUSION_IPC_SOCKET, sizeof(addr.sun_path) - 1);
    unlink(FUSION_IPC_SOCKET);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    chmod(FUSION_IPC_SOCKET, 0666);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    return fd;
}

static void ipc_process_commands(FusionService *svc) {
    char buf[IPC_BUF_SIZE];
    ssize_t n;
    while ((n = recv(svc->ipc_fd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        int mode = atoi(buf);
        if (mode >= 0 && mode <= 4) {
            ir_overlay_set_mode(&svc->overlay, (IROverlayMode)mode);
            hud_state_set_mode(&svc->hud, mode);
        }
    }
}

static void kalman_update_from_pose(FusionService *svc, Mat4 T_observed) {
    float obs[6];
    se3_to_rotvec(T_observed, &obs[0], &obs[1], &obs[2], &obs[3], &obs[4], &obs[5]);
    kalman6_predict(&svc->kalman);
    kalman6_update(&svc->kalman, obs);
    float state[6];
    kalman6_get_state(&svc->kalman, state);
    svc->cal.T_ir_to_headset = se3_from_rotvec(state[0], state[1], state[2],
                                                state[3], state[4], state[5]);
}

int fusion_service_init(FusionService *svc, const char *cal_path,
                        int ir_vendor, int ir_product,
                        int ir_width, int ir_height, int ir_fps) {
    memset(svc, 0, sizeof(*svc));

    if (calibration_load(&svc->cal, cal_path) != 0) {
        fprintf(stderr, "calibration load failed: %s\n", cal_path);
        return -1;
    }

    if (vk_context_create(&svc->vk) != 0) {
        fprintf(stderr, "vulkan context creation failed\n");
        return -1;
    }

    int img_w = 1280, img_h = 960;
    if (fusion_dispatch_init(&svc->dispatch, &svc->vk, img_w, img_h,
                              ir_warp_spirv,   ir_warp_spirv_size,
                              composite_spirv, composite_spirv_size,
                              edge_spirv,      edge_spirv_size,
                              hud_spirv,       hud_spirv_size,
                              lens_spirv,      lens_spirv_size) != 0) {
        vk_context_destroy(&svc->vk);
        return -1;
    }

    if (ts_ring_init(&svc->ir_ring, ir_width, ir_height, 1) != 0) {
        fusion_dispatch_destroy(&svc->dispatch);
        vk_context_destroy(&svc->vk);
        return -1;
    }

    if (uvc_camera_open(&svc->ir_cam, ir_vendor, ir_product,
                        ir_width, ir_height, ir_fps,
                        ir_frame_callback, svc) != 0) {
        fprintf(stderr, "IR camera open failed (vendor=0x%04x product=0x%04x)\n",
                ir_vendor, ir_product);
        ts_ring_destroy(&svc->ir_ring);
        fusion_dispatch_destroy(&svc->dispatch);
        vk_context_destroy(&svc->vk);
        return -1;
    }

    float init_rv[6];
    se3_to_rotvec(svc->cal.T_ir_to_headset,
                  &init_rv[0], &init_rv[1], &init_rv[2],
                  &init_rv[3], &init_rv[4], &init_rv[5]);
    kalman6_init(&svc->kalman, 1e-5f, 1e-3f);
    kalman6_update(&svc->kalman, init_rv);

    ir_overlay_init(&svc->overlay);
    hud_state_init(&svc->hud);
    lens_coeffs_init_quest3(&svc->lens);

    svc->depth_params.focal_px     = svc->cal.K_rgb_left.fx;
    svc->depth_params.baseline_m   = svc->cal.baseline_m;
    svc->depth_params.win_size     = DEPTH_WIN_SIZE;
    svc->depth_params.max_disparity = DEPTH_MAX_DISP;

    if (pipe(svc->btn_pipe) != 0) {
        fprintf(stderr, "button pipe creation failed\n");
    } else {
        fcntl(svc->btn_pipe[0], F_SETFL, O_NONBLOCK);
        char btn_dev[64];
        if (button_input_find_device(btn_dev, sizeof(btn_dev)) == 0) {
            if (button_input_open(&svc->btn, btn_dev,
                                  button_event_callback, svc) != 0) {
                fprintf(stderr, "button input open failed: %s\n", btn_dev);
            }
        } else {
            fprintf(stderr, "no button input device found; volume/power buttons disabled\n");
        }
    }

    svc->ipc_fd  = ipc_server_open();
    svc->running = 1;
    return 0;
}

void fusion_service_run(FusionService *svc) {
    int img_w    = svc->dispatch.width;
    int img_h    = svc->dispatch.height;
    int n_pixels = img_w * img_h;

    float   *depth_buf    = (float   *)malloc(n_pixels * sizeof(float));
    uint8_t *ir_aligned   = (uint8_t *)malloc(svc->ir_ring.frame_bytes);

    if (!depth_buf || !ir_aligned) {
        fprintf(stderr, "memory allocation failed in fusion_service_run\n");
        free(depth_buf); free(ir_aligned);
        return;
    }

    VKImage rgb_left_img, rgb_right_img, depth_img, output_img;
    memset(&rgb_left_img,  0, sizeof(rgb_left_img));
    memset(&rgb_right_img, 0, sizeof(rgb_right_img));
    memset(&depth_img,     0, sizeof(depth_img));
    memset(&output_img,    0, sizeof(output_img));

    if (vk_image_create(&svc->vk, (uint32_t)img_w, (uint32_t)img_h,
                        VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_STORAGE_BIT, &rgb_left_img)  != 0) goto cleanup;
    if (vk_image_create(&svc->vk, (uint32_t)img_w, (uint32_t)img_h,
                        VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_STORAGE_BIT, &rgb_right_img) != 0) goto cleanup;
    if (vk_image_create(&svc->vk, (uint32_t)img_w, (uint32_t)img_h,
                        VK_FORMAT_R32_SFLOAT,
                        VK_IMAGE_USAGE_STORAGE_BIT, &depth_img)     != 0) goto cleanup;
    if (vk_image_create(&svc->vk, (uint32_t)img_w, (uint32_t)img_h,
                        VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_STORAGE_BIT, &output_img)    != 0) goto cleanup;

    while (svc->running) {
        if (svc->btn_pipe[0] >= 0) process_button_pipe(svc);
        if (svc->ipc_fd >= 0)      ipc_process_commands(svc);

        TSFrame nearest_ir;
        if (ts_ring_get_nearest(&svc->ir_ring, 0, &nearest_ir) < 0) {
            usleep(1000);
            continue;
        }

        int ir_bytes = svc->ir_ring.frame_bytes;
        if (nearest_ir.data)
            memcpy(ir_aligned, nearest_ir.data, (size_t)ir_bytes);

        float ir_intensity = hud_compute_ir_intensity(ir_aligned, ir_bytes);
        hud_state_set_ir_intensity(&svc->hud, ir_intensity);

        float a, b, g, dk;
        ir_overlay_get_coefficients(&svc->overlay, &a, &b, &g, &dk);

        FusionPushConstants fpc;
        fpc.alpha   = a; fpc.beta = b; fpc.gamma = g; fpc.depth_k = dk;
        memcpy(fpc.H_inv, svc->cal.H_inv, 9 * sizeof(float));
        fpc.out_w = img_w; fpc.out_h = img_h;

        HUDPushConstants hpc;
        hud_get_push_constants(&svc->hud, img_w, img_h, &hpc);

        LensPushConstants lpc;
        lens_get_push_constants(&svc->lens, img_w, img_h, &lpc);

        fusion_dispatch_run(&svc->dispatch,
                            &rgb_left_img, &rgb_left_img, &depth_img, &output_img,
                            &fpc, &hpc, &lpc);

        usleep(11000);
    }

cleanup:
    vk_image_destroy(&svc->vk, &rgb_left_img);
    vk_image_destroy(&svc->vk, &rgb_right_img);
    vk_image_destroy(&svc->vk, &depth_img);
    vk_image_destroy(&svc->vk, &output_img);
    free(depth_buf);
    free(ir_aligned);
}

void fusion_service_shutdown(FusionService *svc) {
    svc->running = 0;
    button_input_close(&svc->btn);
    if (svc->btn_pipe[0] >= 0) { close(svc->btn_pipe[0]); svc->btn_pipe[0] = -1; }
    if (svc->btn_pipe[1] >= 0) { close(svc->btn_pipe[1]); svc->btn_pipe[1] = -1; }
    uvc_camera_close(&svc->ir_cam);
    if (svc->ipc_fd >= 0) { close(svc->ipc_fd); svc->ipc_fd = -1; }
    unlink(FUSION_IPC_SOCKET);
    fusion_dispatch_destroy(&svc->dispatch);
    ts_ring_destroy(&svc->ir_ring);
    vk_context_destroy(&svc->vk);
}