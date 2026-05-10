#include "vk_dispatch.h"
#include <string.h>
#include <stdlib.h>

static void barrier(VkCommandBuffer cmd) {
    VkMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &b, 0, NULL, 0, NULL);
}

static void transition_read(VkCommandBuffer cmd, VKImage *img) {
    vk_image_transition(cmd, img, VK_IMAGE_LAYOUT_GENERAL,
        0, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

static void transition_write(VkCommandBuffer cmd, VKImage *img) {
    vk_image_transition(cmd, img, VK_IMAGE_LAYOUT_GENERAL,
        0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

int fusion_dispatch_init(FusionDispatch *fd, VKContext *ctx, int w, int h,
                         const uint32_t *warp_spirv,  size_t warp_bytes,
                         const uint32_t *comp_spirv,  size_t comp_bytes,
                         const uint32_t *edge_spirv,  size_t edge_bytes,
                         const uint32_t *hud_spirv,   size_t hud_bytes,
                         const uint32_t *lens_spirv,  size_t lens_bytes) {
    memset(fd, 0, sizeof(*fd));
    fd->ctx    = ctx;
    fd->width  = w;
    fd->height = h;

    if (vk_image_create(ctx, (uint32_t)w, (uint32_t)h, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_STORAGE_BIT, &fd->ir_warped) != 0) goto fail;
    if (vk_image_create(ctx, (uint32_t)w, (uint32_t)h, VK_FORMAT_R32_SFLOAT,
                        VK_IMAGE_USAGE_STORAGE_BIT, &fd->edge_map) != 0) goto fail;
    if (vk_image_create(ctx, (uint32_t)w, (uint32_t)h, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_STORAGE_BIT, &fd->composite_buf) != 0) goto fail;
    if (vk_image_create(ctx, (uint32_t)w, (uint32_t)h, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_STORAGE_BIT, &fd->hud_buf) != 0) goto fail;

    typedef struct { float h[9]; int w, hh; }     WarpPC;
    typedef struct { float a,b,g,dk; int w, hh; } CompPC;
    typedef struct { int w, hh; }                  EdgePC;
    typedef struct { int m; float ir, t; int ht, w, hh; } HUDPC;
    typedef struct { float k1,k2; int w, hh; }    LensPC;

    if (vk_pipeline_create(ctx, warp_spirv, warp_bytes, 2,
                           (int)sizeof(WarpPC), &fd->warp_pipe) != 0) goto fail;
    if (vk_pipeline_create(ctx, edge_spirv, edge_bytes, 2,
                           (int)sizeof(EdgePC), &fd->edge_pipe) != 0) goto fail;
    if (vk_pipeline_create(ctx, comp_spirv, comp_bytes, 4,
                           (int)sizeof(CompPC), &fd->composite_pipe) != 0) goto fail;
    if (vk_pipeline_create(ctx, hud_spirv,  hud_bytes,  2,
                           (int)sizeof(HUDPC), &fd->hud_pipe) != 0) goto fail;
    if (vk_pipeline_create(ctx, lens_spirv, lens_bytes, 2,
                           (int)sizeof(LensPC), &fd->lens_pipe) != 0) goto fail;
    return 0;

fail:
    fusion_dispatch_destroy(fd);
    return -1;
}

void fusion_dispatch_destroy(FusionDispatch *fd) {
    if (!fd->ctx) return;
    vk_pipeline_destroy(fd->ctx, &fd->warp_pipe);
    vk_pipeline_destroy(fd->ctx, &fd->edge_pipe);
    vk_pipeline_destroy(fd->ctx, &fd->composite_pipe);
    vk_pipeline_destroy(fd->ctx, &fd->hud_pipe);
    vk_pipeline_destroy(fd->ctx, &fd->lens_pipe);
    vk_image_destroy(fd->ctx, &fd->ir_warped);
    vk_image_destroy(fd->ctx, &fd->edge_map);
    vk_image_destroy(fd->ctx, &fd->composite_buf);
    vk_image_destroy(fd->ctx, &fd->hud_buf);
    memset(fd, 0, sizeof(*fd));
}

int fusion_dispatch_run(FusionDispatch *fd,
                        VKImage *rgb, VKImage *ir_src, VKImage *depth,
                        VKImage *output,
                        const FusionPushConstants *pc,
                        const HUDPushConstants    *hpc,
                        const LensPushConstants   *lpc) {
    VkCommandBuffer cmd = vk_alloc_command_buffer(fd->ctx);
    if (!cmd) return -1;

    VkCommandBufferBeginInfo bi;
    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    transition_read(cmd, rgb);
    transition_read(cmd, ir_src);
    transition_read(cmd, depth);
    transition_write(cmd, &fd->ir_warped);
    transition_write(cmd, &fd->edge_map);
    transition_write(cmd, &fd->composite_buf);
    transition_write(cmd, &fd->hud_buf);
    transition_write(cmd, output);

    uint32_t gx = ((uint32_t)fd->width  + 15) / 16;
    uint32_t gy = ((uint32_t)fd->height + 15) / 16;

    struct { float h[9]; int w, hh; } warp_pc;
    memcpy(warp_pc.h, pc->H_inv, 9 * sizeof(float));
    warp_pc.w = fd->width; warp_pc.hh = fd->height;
    vk_pipeline_bind_image(fd->ctx, &fd->warp_pipe, 0, ir_src);
    vk_pipeline_bind_image(fd->ctx, &fd->warp_pipe, 1, &fd->ir_warped);
    vk_pipeline_dispatch(cmd, &fd->warp_pipe, &warp_pc, sizeof(warp_pc), gx, gy);
    barrier(cmd);

    struct { int w, hh; } edge_pc;
    edge_pc.w = fd->width; edge_pc.hh = fd->height;
    vk_pipeline_bind_image(fd->ctx, &fd->edge_pipe, 0, &fd->ir_warped);
    vk_pipeline_bind_image(fd->ctx, &fd->edge_pipe, 1, &fd->edge_map);
    vk_pipeline_dispatch(cmd, &fd->edge_pipe, &edge_pc, sizeof(edge_pc), gx, gy);
    barrier(cmd);

    struct { float a, b, g, dk; int w, hh; } comp_pc;
    comp_pc.a = pc->alpha; comp_pc.b = pc->beta;
    comp_pc.g = pc->gamma; comp_pc.dk = pc->depth_k;
    comp_pc.w = fd->width; comp_pc.hh = fd->height;
    vk_pipeline_bind_image(fd->ctx, &fd->composite_pipe, 0, rgb);
    vk_pipeline_bind_image(fd->ctx, &fd->composite_pipe, 1, &fd->ir_warped);
    vk_pipeline_bind_image(fd->ctx, &fd->composite_pipe, 2, depth);
    vk_pipeline_bind_image(fd->ctx, &fd->composite_pipe, 3, &fd->composite_buf);
    vk_pipeline_dispatch(cmd, &fd->composite_pipe, &comp_pc, sizeof(comp_pc), gx, gy);
    barrier(cmd);

    struct { int m; float ir, t; int ht, w, hh; } hud_pc;
    hud_pc.m  = hpc->mode;
    hud_pc.ir = hpc->ir_intensity;
    hud_pc.t  = hpc->temperature;
    hud_pc.ht = hpc->has_temperature;
    hud_pc.w  = fd->width;
    hud_pc.hh = fd->height;
    vk_pipeline_bind_image(fd->ctx, &fd->hud_pipe, 0, &fd->composite_buf);
    vk_pipeline_bind_image(fd->ctx, &fd->hud_pipe, 1, &fd->hud_buf);
    vk_pipeline_dispatch(cmd, &fd->hud_pipe, &hud_pc, sizeof(hud_pc), gx, gy);
    barrier(cmd);

    struct { float k1, k2; int w, hh; } lens_pc;
    lens_pc.k1 = lpc->k1; lens_pc.k2 = lpc->k2;
    lens_pc.w  = fd->width; lens_pc.hh = fd->height;
    vk_pipeline_bind_image(fd->ctx, &fd->lens_pipe, 0, &fd->hud_buf);
    vk_pipeline_bind_image(fd->ctx, &fd->lens_pipe, 1, output);
    vk_pipeline_dispatch(cmd, &fd->lens_pipe, &lens_pc, sizeof(lens_pc), gx, gy);

    vkEndCommandBuffer(cmd);
    int ret = vk_submit_and_wait(fd->ctx, cmd);
    vk_free_command_buffer(fd->ctx, cmd);
    return ret;
}