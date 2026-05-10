#include "compositor.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int create_instance(XRCompositor *xr) {
    const char *extensions[] = {
        XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME,
    };
    XrInstanceCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.type = XR_TYPE_INSTANCE_CREATE_INFO;
    strncpy(ci.applicationInfo.applicationName, "IRFusion", XR_MAX_APPLICATION_NAME_SIZE - 1);
    strncpy(ci.applicationInfo.engineName, "IRFusionEngine", XR_MAX_ENGINE_NAME_SIZE - 1);
    ci.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ci.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    ci.enabledExtensionNames = extensions;
    return (xrCreateInstance(&ci, &xr->instance) == XR_SUCCESS) ? 0 : -1;
}

static int get_system(XRCompositor *xr) {
    XrSystemGetInfo sgi;
    memset(&sgi, 0, sizeof(sgi));
    sgi.type = XR_TYPE_SYSTEM_GET_INFO;
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    return (xrGetSystem(xr->instance, &sgi, &xr->system_id) == XR_SUCCESS) ? 0 : -1;
}

static int create_session(XRCompositor *xr) {
    XrGraphicsBindingVulkan2KHR vk_binding;
    memset(&vk_binding, 0, sizeof(vk_binding));
    vk_binding.type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR;
    vk_binding.instance = xr->vk_ctx->instance;
    vk_binding.physicalDevice = xr->vk_ctx->physical_device;
    vk_binding.device = xr->vk_ctx->device;
    vk_binding.queueFamilyIndex = xr->vk_ctx->compute_queue_family;
    vk_binding.queueIndex = 0;

    XrSessionCreateInfo sci;
    memset(&sci, 0, sizeof(sci));
    sci.type = XR_TYPE_SESSION_CREATE_INFO;
    sci.next = &vk_binding;
    sci.systemId = xr->system_id;
    return (xrCreateSession(xr->instance, &sci, &xr->session) == XR_SUCCESS) ? 0 : -1;
}

static int create_reference_space(XRCompositor *xr) {
    XrReferenceSpaceCreateInfo rsci;
    memset(&rsci, 0, sizeof(rsci));
    rsci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace.orientation.w = 1.0f;
    return (xrCreateReferenceSpace(xr->session, &rsci, &xr->local_space) == XR_SUCCESS) ? 0 : -1;
}

static int create_swapchain(XRCompositor *xr) {
    XrSwapchainCreateInfo sci;
    memset(&sci, 0, sizeof(sci));
    sci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    sci.format = VK_FORMAT_R8G8B8A8_UNORM;
    sci.sampleCount = 1;
    sci.width = 1280;
    sci.height = 960;
    sci.faceCount = 1;
    sci.arraySize = 1;
    sci.mipCount = 1;
    xr->swapchain_width = 1280;
    xr->swapchain_height = 960;

    if (xrCreateSwapchain(xr->session, &sci, &xr->swapchain) != XR_SUCCESS) return -1;

    uint32_t count = 0;
    xrEnumerateSwapchainImages(xr->swapchain, 0, &count, NULL);
    if (count == 0 || count > XR_SWAPCHAIN_IMAGES) return -1;
    xr->swapchain_image_count = count;

    XrSwapchainImageVulkan2KHR imgs[XR_SWAPCHAIN_IMAGES];
    uint32_t i;
    for (i = 0; i < count; i++) {
        memset(&imgs[i], 0, sizeof(imgs[i]));
        imgs[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR;
    }
    xrEnumerateSwapchainImages(xr->swapchain, count, &count, (XrSwapchainImageBaseHeader *)imgs);
    for (i = 0; i < count; i++) {
        xr->swapchain_images[i] = imgs[i].image;
        VkImageViewCreateInfo view_ci;
        memset(&view_ci, 0, sizeof(view_ci));
        view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image = imgs[i].image;
        view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
        view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.levelCount = 1;
        view_ci.subresourceRange.layerCount = 1;
        vkCreateImageView(xr->vk_ctx->device, &view_ci, NULL, &xr->swapchain_views[i]);
    }
    return 0;
}

int xr_compositor_init(XRCompositor *xr, VKContext *vk_ctx) {
    memset(xr, 0, sizeof(*xr));
    xr->vk_ctx = vk_ctx;
    if (create_instance(xr) != 0) return -1;
    if (get_system(xr) != 0) { xrDestroyInstance(xr->instance); return -1; }
    if (create_session(xr) != 0) { xrDestroyInstance(xr->instance); return -1; }
    if (create_reference_space(xr) != 0) {
        xrDestroySession(xr->session);
        xrDestroyInstance(xr->instance);
        return -1;
    }
    if (create_swapchain(xr) != 0) {
        xrDestroySpace(xr->local_space);
        xrDestroySession(xr->session);
        xrDestroyInstance(xr->instance);
        return -1;
    }

    XrSessionBeginInfo sbi;
    memset(&sbi, 0, sizeof(sbi));
    sbi.type = XR_TYPE_SESSION_BEGIN_INFO;
    sbi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    xrBeginSession(xr->session, &sbi);
    xr->running = 1;
    return 0;
}

void xr_compositor_destroy(XRCompositor *xr) {
    uint32_t i;
    for (i = 0; i < xr->swapchain_image_count; i++)
        if (xr->swapchain_views[i])
            vkDestroyImageView(xr->vk_ctx->device, xr->swapchain_views[i], NULL);
    if (xr->swapchain) xrDestroySwapchain(xr->swapchain);
    if (xr->local_space) xrDestroySpace(xr->local_space);
    if (xr->session) xrDestroySession(xr->session);
    if (xr->instance) xrDestroyInstance(xr->instance);
    memset(xr, 0, sizeof(*xr));
}

int xr_compositor_begin_frame(XRCompositor *xr, XrTime *predicted_time) {
    XrFrameState fs;
    memset(&fs, 0, sizeof(fs));
    fs.type = XR_TYPE_FRAME_STATE;
    XrFrameWaitInfo fwi;
    memset(&fwi, 0, sizeof(fwi));
    fwi.type = XR_TYPE_FRAME_WAIT_INFO;
    if (xrWaitFrame(xr->session, &fwi, &fs) != XR_SUCCESS) return -1;
    *predicted_time = fs.predictedDisplayTime;
    XrFrameBeginInfo fbi;
    memset(&fbi, 0, sizeof(fbi));
    fbi.type = XR_TYPE_FRAME_BEGIN_INFO;
    xrBeginFrame(xr->session, &fbi);
    return 0;
}

int xr_compositor_acquire_image(XRCompositor *xr, uint32_t *index_out) {
    XrSwapchainImageAcquireInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    if (xrAcquireSwapchainImage(xr->swapchain, &ai, index_out) != XR_SUCCESS) return -1;
    XrSwapchainImageWaitInfo wi;
    memset(&wi, 0, sizeof(wi));
    wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    wi.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(xr->swapchain, &wi);
    return 0;
}

int xr_compositor_release_and_submit(XRCompositor *xr, uint32_t index, XrTime predicted_time) {
    (void)index;
    XrSwapchainImageReleaseInfo ri;
    memset(&ri, 0, sizeof(ri));
    ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
    xrReleaseSwapchainImage(xr->swapchain, &ri);

    XrCompositionLayerQuad layer;
    memset(&layer, 0, sizeof(layer));
    layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    layer.space = xr->local_space;
    layer.subImage.swapchain = xr->swapchain;
    layer.subImage.imageRect.extent.width  = (int32_t)xr->swapchain_width;
    layer.subImage.imageRect.extent.height = (int32_t)xr->swapchain_height;
    layer.pose.orientation.w = 1.0f;
    layer.size.width = 2.0f;
    layer.size.height = 1.5f;

    const XrCompositionLayerBaseHeader *layers[1];
    layers[0] = (const XrCompositionLayerBaseHeader *)&layer;

    XrFrameEndInfo fei;
    memset(&fei, 0, sizeof(fei));
    fei.type = XR_TYPE_FRAME_END_INFO;
    fei.displayTime = predicted_time;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
    fei.layerCount = 1;
    fei.layers = layers;
    return (xrEndFrame(xr->session, &fei) == XR_SUCCESS) ? 0 : -1;
}

void xr_compositor_get_swapchain_size(const XRCompositor *xr, uint32_t *w, uint32_t *h) {
    *w = xr->swapchain_width;
    *h = xr->swapchain_height;
}