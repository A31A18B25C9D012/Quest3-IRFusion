#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include "vk_context.h"
#ifdef __ANDROID__
#include <android/hardware_buffer.h>
#endif

typedef struct {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    uint32_t width;
    uint32_t height;
    VkFormat format;
    VkImageLayout current_layout;
} VKImage;

int vk_image_create(VKContext *ctx, uint32_t w, uint32_t h, VkFormat fmt,
                    VkImageUsageFlags usage, VKImage *out);
void vk_image_destroy(VKContext *ctx, VKImage *img);
void vk_image_transition(VkCommandBuffer cmd, VKImage *img,
                         VkImageLayout new_layout,
                         VkAccessFlags src_access, VkAccessFlags dst_access,
                         VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage);

#ifdef __ANDROID__
int vk_image_from_ahardware_buffer(VKContext *ctx, AHardwareBuffer *buf,
                                   uint32_t w, uint32_t h, VkFormat fmt, VKImage *out);
#endif

#endif