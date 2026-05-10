#ifndef VK_PIPELINE_H
#define VK_PIPELINE_H

#include "vk_context.h"

typedef struct {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSet desc_set;
    int binding_count;
} VKComputePipeline;

int vk_pipeline_create(VKContext *ctx, const uint32_t *spirv, size_t spirv_bytes,
                       int n_images, int n_push_constant_bytes, VKComputePipeline *out);
void vk_pipeline_destroy(VKContext *ctx, VKComputePipeline *pipe);
void vk_pipeline_bind_image(VKContext *ctx, VKComputePipeline *pipe, int binding, VKImage *img);
void vk_pipeline_dispatch(VkCommandBuffer cmd, VKComputePipeline *pipe,
                           const void *push_data, size_t push_bytes,
                           uint32_t groups_x, uint32_t groups_y);

#endif