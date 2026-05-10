#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include <vulkan/vulkan.h>
#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue compute_queue;
    uint32_t compute_queue_family;
    VkCommandPool command_pool;
    VkDescriptorPool descriptor_pool;
    PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferProperties;
    PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBuffer;
} VKContext;

int vk_context_create(VKContext *ctx);
void vk_context_destroy(VKContext *ctx);
VkCommandBuffer vk_alloc_command_buffer(VKContext *ctx);
void vk_free_command_buffer(VKContext *ctx, VkCommandBuffer cmd);
int vk_submit_and_wait(VKContext *ctx, VkCommandBuffer cmd);

#endif