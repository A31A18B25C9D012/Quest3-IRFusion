#include "vk_context.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int find_compute_queue(VkPhysicalDevice phys, uint32_t *family_out) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, NULL);
    if (count == 0) return -1;
    VkQueueFamilyProperties *props = (VkQueueFamilyProperties *)malloc(count * sizeof(*props));
    if (!props) return -1;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, props);
    uint32_t i;
    for (i = 0; i < count; i++) {
        if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            *family_out = i;
            free(props);
            return 0;
        }
    }
    free(props);
    return -1;
}

int vk_context_create(VKContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));

    const char *inst_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __ANDROID__
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
#endif
    };
    uint32_t n_inst_exts = sizeof(inst_exts) / sizeof(inst_exts[0]);

    VkApplicationInfo app_info;
    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo inst_ci;
    memset(&inst_ci, 0, sizeof(inst_ci));
    inst_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_ci.pApplicationInfo = &app_info;
    inst_ci.enabledExtensionCount = n_inst_exts;
    inst_ci.ppEnabledExtensionNames = inst_exts;

    if (vkCreateInstance(&inst_ci, NULL, &ctx->instance) != VK_SUCCESS)
        return -1;

    uint32_t dev_count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &dev_count, NULL);
    if (dev_count == 0) { vkDestroyInstance(ctx->instance, NULL); return -1; }
    VkPhysicalDevice *devs = (VkPhysicalDevice *)malloc(dev_count * sizeof(*devs));
    if (!devs) { vkDestroyInstance(ctx->instance, NULL); return -1; }
    vkEnumeratePhysicalDevices(ctx->instance, &dev_count, devs);
    ctx->physical_device = devs[0];
    free(devs);

    if (find_compute_queue(ctx->physical_device, &ctx->compute_queue_family) != 0) {
        vkDestroyInstance(ctx->instance, NULL);
        return -1;
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo q_ci;
    memset(&q_ci, 0, sizeof(q_ci));
    q_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    q_ci.queueFamilyIndex = ctx->compute_queue_family;
    q_ci.queueCount = 1;
    q_ci.pQueuePriorities = &priority;

    const char *dev_exts[] = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
#ifdef __ANDROID__
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
#endif
    };
    uint32_t n_dev_exts = sizeof(dev_exts) / sizeof(dev_exts[0]);

    VkPhysicalDeviceFeatures features;
    memset(&features, 0, sizeof(features));

    VkDeviceCreateInfo dev_ci;
    memset(&dev_ci, 0, sizeof(dev_ci));
    dev_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dev_ci.queueCreateInfoCount = 1;
    dev_ci.pQueueCreateInfos = &q_ci;
    dev_ci.enabledExtensionCount = n_dev_exts;
    dev_ci.ppEnabledExtensionNames = dev_exts;
    dev_ci.pEnabledFeatures = &features;

    if (vkCreateDevice(ctx->physical_device, &dev_ci, NULL, &ctx->device) != VK_SUCCESS) {
        vkDestroyInstance(ctx->instance, NULL);
        return -1;
    }

    vkGetDeviceQueue(ctx->device, ctx->compute_queue_family, 0, &ctx->compute_queue);

    VkCommandPoolCreateInfo pool_ci;
    memset(&pool_ci, 0, sizeof(pool_ci));
    pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.queueFamilyIndex = ctx->compute_queue_family;
    pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(ctx->device, &pool_ci, NULL, &ctx->command_pool) != VK_SUCCESS) {
        vkDestroyDevice(ctx->device, NULL);
        vkDestroyInstance(ctx->instance, NULL);
        return -1;
    }

    VkDescriptorPoolSize pool_sizes[1];
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    pool_sizes[0].descriptorCount = 64;
    VkDescriptorPoolCreateInfo dp_ci;
    memset(&dp_ci, 0, sizeof(dp_ci));
    dp_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dp_ci.poolSizeCount = 1;
    dp_ci.pPoolSizes = pool_sizes;
    dp_ci.maxSets = 16;
    dp_ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    vkCreateDescriptorPool(ctx->device, &dp_ci, NULL, &ctx->descriptor_pool);

#ifdef __ANDROID__
    ctx->vkGetAndroidHardwareBufferProperties =
        (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)
        vkGetDeviceProcAddr(ctx->device, "vkGetAndroidHardwareBufferPropertiesANDROID");
    ctx->vkGetMemoryAndroidHardwareBuffer =
        (PFN_vkGetMemoryAndroidHardwareBufferANDROID)
        vkGetDeviceProcAddr(ctx->device, "vkGetMemoryAndroidHardwareBufferANDROID");
#endif

    return 0;
}

void vk_context_destroy(VKContext *ctx) {
    if (ctx->descriptor_pool) vkDestroyDescriptorPool(ctx->device, ctx->descriptor_pool, NULL);
    if (ctx->command_pool) vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
    if (ctx->device) vkDestroyDevice(ctx->device, NULL);
    if (ctx->instance) vkDestroyInstance(ctx->instance, NULL);
    memset(ctx, 0, sizeof(*ctx));
}

VkCommandBuffer vk_alloc_command_buffer(VKContext *ctx) {
    VkCommandBufferAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = ctx->command_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(ctx->device, &ai, &cmd);
    return cmd;
}

void vk_free_command_buffer(VKContext *ctx, VkCommandBuffer cmd) {
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd);
}

int vk_submit_and_wait(VKContext *ctx, VkCommandBuffer cmd) {
    VkFenceCreateInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(ctx->device, &fi, NULL, &fence) != VK_SUCCESS) return -1;

    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    VkResult r = vkQueueSubmit(ctx->compute_queue, 1, &si, fence);
    if (r == VK_SUCCESS)
        vkWaitForFences(ctx->device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(ctx->device, fence, NULL);
    return (r == VK_SUCCESS) ? 0 : -1;
}