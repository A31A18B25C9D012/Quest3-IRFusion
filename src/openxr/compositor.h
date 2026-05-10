#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "../vulkan/vk_context.h"
#include "../vulkan/vk_image.h"

#define XR_SWAPCHAIN_IMAGES 3

typedef struct {
    XrInstance instance;
    XrSystemId system_id;
    XrSession session;
    XrSpace local_space;
    XrSwapchain swapchain;
    VkImage swapchain_images[XR_SWAPCHAIN_IMAGES];
    VkImageView swapchain_views[XR_SWAPCHAIN_IMAGES];
    uint32_t swapchain_width;
    uint32_t swapchain_height;
    uint32_t swapchain_image_count;
    XrCompositionLayerQuad layer;
    VKContext *vk_ctx;
    int running;
} XRCompositor;

int xr_compositor_init(XRCompositor *xr, VKContext *vk_ctx);
void xr_compositor_destroy(XRCompositor *xr);
int xr_compositor_begin_frame(XRCompositor *xr, XrTime *predicted_time);
int xr_compositor_acquire_image(XRCompositor *xr, uint32_t *index_out);
int xr_compositor_release_and_submit(XRCompositor *xr, uint32_t index, XrTime predicted_time);
void xr_compositor_get_swapchain_size(const XRCompositor *xr, uint32_t *w, uint32_t *h);

#endif