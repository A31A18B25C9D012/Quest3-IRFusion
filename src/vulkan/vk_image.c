#include "vk_image.h"
#include <string.h>
#include <stdlib.h>

static uint32_t find_memory_type(VkPhysicalDevice phys, uint32_t type_bits,
                                  VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);
    uint32_t i;
    for (i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

int vk_image_create(VKContext *ctx, uint32_t w, uint32_t h, VkFormat fmt,
                    VkImageUsageFlags usage, VKImage *out) {
    memset(out, 0, sizeof(*out));
    out->width = w; out->height = h; out->format = fmt;

    VkImageCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent.width = w; ci.extent.height = h; ci.extent.depth = 1;
    ci.mipLevels = 1; ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    out->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(ctx->device, &ci, NULL, &out->image) != VK_SUCCESS) return -1;

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(ctx->device, out->image, &mem_req);
    uint32_t mem_type = find_memory_type(ctx->physical_device, mem_req.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type == UINT32_MAX) {
        vkDestroyImage(ctx->device, out->image, NULL);
        return -1;
    }

    VkMemoryAllocateInfo alloc_i;
    memset(&alloc_i, 0, sizeof(alloc_i));
    alloc_i.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_i.allocationSize = mem_req.size;
    alloc_i.memoryTypeIndex = mem_type;
    if (vkAllocateMemory(ctx->device, &alloc_i, NULL, &out->memory) != VK_SUCCESS) {
        vkDestroyImage(ctx->device, out->image, NULL);
        return -1;
    }
    vkBindImageMemory(ctx->device, out->image, out->memory, 0);

    VkImageViewCreateInfo view_ci;
    memset(&view_ci, 0, sizeof(view_ci));
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image = out->image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = fmt;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx->device, &view_ci, NULL, &out->view) != VK_SUCCESS) {
        vkFreeMemory(ctx->device, out->memory, NULL);
        vkDestroyImage(ctx->device, out->image, NULL);
        return -1;
    }
    return 0;
}

void vk_image_destroy(VKContext *ctx, VKImage *img) {
    if (img->view) vkDestroyImageView(ctx->device, img->view, NULL);
    if (img->memory) vkFreeMemory(ctx->device, img->memory, NULL);
    if (img->image) vkDestroyImage(ctx->device, img->image, NULL);
    memset(img, 0, sizeof(*img));
}

void vk_image_transition(VkCommandBuffer cmd, VKImage *img,
                         VkImageLayout new_layout,
                         VkAccessFlags src_access, VkAccessFlags dst_access,
                         VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = img->current_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
    img->current_layout = new_layout;
}

#ifdef __ANDROID__
int vk_image_from_ahardware_buffer(VKContext *ctx, AHardwareBuffer *buf,
                                   uint32_t w, uint32_t h, VkFormat fmt, VKImage *out) {
    memset(out, 0, sizeof(*out));
    out->width = w; out->height = h; out->format = fmt;

    VkAndroidHardwareBufferPropertiesANDROID ahb_props;
    memset(&ahb_props, 0, sizeof(ahb_props));
    ahb_props.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    if (!ctx->vkGetAndroidHardwareBufferProperties) return -1;
    if (ctx->vkGetAndroidHardwareBufferProperties(ctx->device, buf, &ahb_props) != VK_SUCCESS)
        return -1;

    VkExternalMemoryImageCreateInfo ext_mem_ci;
    memset(&ext_mem_ci, 0, sizeof(ext_mem_ci));
    ext_mem_ci.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    ext_mem_ci.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkImageCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.pNext = &ext_mem_ci;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent.width = w; ci.extent.height = h; ci.extent.depth = 1;
    ci.mipLevels = 1; ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    out->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx->device, &ci, NULL, &out->image) != VK_SUCCESS) return -1;

    VkImportAndroidHardwareBufferInfoANDROID import_info;
    memset(&import_info, 0, sizeof(import_info));
    import_info.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    import_info.buffer = buf;

    VkMemoryAllocateInfo alloc_i;
    memset(&alloc_i, 0, sizeof(alloc_i));
    alloc_i.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_i.pNext = &import_info;
    alloc_i.allocationSize = ahb_props.allocationSize;
    alloc_i.memoryTypeIndex = 0;
    uint32_t bits = ahb_props.memoryTypeBits;
    while (bits && !(bits & 1)) { alloc_i.memoryTypeIndex++; bits >>= 1; }
    if (vkAllocateMemory(ctx->device, &alloc_i, NULL, &out->memory) != VK_SUCCESS) {
        vkDestroyImage(ctx->device, out->image, NULL);
        return -1;
    }
    vkBindImageMemory(ctx->device, out->image, out->memory, 0);

    VkImageViewCreateInfo view_ci;
    memset(&view_ci, 0, sizeof(view_ci));
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image = out->image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = fmt;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.layerCount = 1;
    vkCreateImageView(ctx->device, &view_ci, NULL, &out->view);
    return 0;
}
#endif