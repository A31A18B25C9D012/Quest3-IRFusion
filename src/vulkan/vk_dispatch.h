#ifndef VK_DISPATCH_H
#define VK_DISPATCH_H

#include "vk_context.h"
#include "vk_image.h"
#include "vk_pipeline.h"

typedef struct {
    float alpha;
    float beta;
    float gamma;
    float depth_k;
    float H_inv[9];
    int   out_w;
    int   out_h;
} FusionPushConstants;

typedef struct {
    int   mode;
    float ir_intensity;
    float temperature;
    int   has_temperature;
    int   width;
    int   height;
} HUDPushConstants;

typedef struct {
    float k1;
    float k2;
    int   width;
    int   height;
} LensPushConstants;

typedef struct {
    VKContext          *ctx;
    VKComputePipeline   warp_pipe;
    VKComputePipeline   edge_pipe;
    VKComputePipeline   composite_pipe;
    VKComputePipeline   hud_pipe;
    VKComputePipeline   lens_pipe;
    VKImage             ir_warped;
    VKImage             edge_map;
    VKImage             composite_buf;
    VKImage             hud_buf;
    int                 width;
    int                 height;
} FusionDispatch;

int fusion_dispatch_init(FusionDispatch *fd, VKContext *ctx, int w, int h,
                         const uint32_t *warp_spirv,  size_t warp_bytes,
                         const uint32_t *comp_spirv,  size_t comp_bytes,
                         const uint32_t *edge_spirv,  size_t edge_bytes,
                         const uint32_t *hud_spirv,   size_t hud_bytes,
                         const uint32_t *lens_spirv,  size_t lens_bytes);

void fusion_dispatch_destroy(FusionDispatch *fd);

int fusion_dispatch_run(FusionDispatch *fd,
                        VKImage *rgb, VKImage *ir_src, VKImage *depth,
                        VKImage *output,
                        const FusionPushConstants *pc,
                        const HUDPushConstants    *hpc,
                        const LensPushConstants   *lpc);

#endif