#include "vk_pipeline.h"
#include <stdlib.h>
#include <string.h>

int vk_pipeline_create(VKContext *ctx, const uint32_t *spirv, size_t spirv_bytes,
                       int n_images, int n_push_constant_bytes, VKComputePipeline *out) {
    memset(out, 0, sizeof(*out));
    out->binding_count = n_images;

    VkShaderModuleCreateInfo sm_ci;
    memset(&sm_ci, 0, sizeof(sm_ci));
    sm_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    sm_ci.codeSize = spirv_bytes;
    sm_ci.pCode = spirv;
    VkShaderModule shader = VK_NULL_HANDLE;
    if (vkCreateShaderModule(ctx->device, &sm_ci, NULL, &shader) != VK_SUCCESS) return -1;

    VkDescriptorSetLayoutBinding *bindings =
        (VkDescriptorSetLayoutBinding *)calloc(n_images, sizeof(*bindings));
    if (!bindings) { vkDestroyShaderModule(ctx->device, shader, NULL); return -1; }
    int i;
    for (i = 0; i < n_images; i++) {
        bindings[i].binding = (uint32_t)i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl_ci;
    memset(&dsl_ci, 0, sizeof(dsl_ci));
    dsl_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = (uint32_t)n_images;
    dsl_ci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(ctx->device, &dsl_ci, NULL, &out->desc_set_layout) != VK_SUCCESS) {
        free(bindings); vkDestroyShaderModule(ctx->device, shader, NULL); return -1;
    }
    free(bindings);

    VkPushConstantRange pc_range;
    pc_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc_range.offset = 0;
    pc_range.size = (uint32_t)n_push_constant_bytes;

    VkPipelineLayoutCreateInfo pl_ci;
    memset(&pl_ci, 0, sizeof(pl_ci));
    pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount = 1;
    pl_ci.pSetLayouts = &out->desc_set_layout;
    if (n_push_constant_bytes > 0) {
        pl_ci.pushConstantRangeCount = 1;
        pl_ci.pPushConstantRanges = &pc_range;
    }
    if (vkCreatePipelineLayout(ctx->device, &pl_ci, NULL, &out->layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(ctx->device, out->desc_set_layout, NULL);
        vkDestroyShaderModule(ctx->device, shader, NULL);
        return -1;
    }

    VkComputePipelineCreateInfo cp_ci;
    memset(&cp_ci, 0, sizeof(cp_ci));
    cp_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cp_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cp_ci.stage.module = shader;
    cp_ci.stage.pName = "main";
    cp_ci.layout = out->layout;
    if (vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &cp_ci, NULL, &out->pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(ctx->device, out->layout, NULL);
        vkDestroyDescriptorSetLayout(ctx->device, out->desc_set_layout, NULL);
        vkDestroyShaderModule(ctx->device, shader, NULL);
        return -1;
    }
    vkDestroyShaderModule(ctx->device, shader, NULL);

    VkDescriptorSetAllocateInfo ds_ai;
    memset(&ds_ai, 0, sizeof(ds_ai));
    ds_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ds_ai.descriptorPool = ctx->descriptor_pool;
    ds_ai.descriptorSetCount = 1;
    ds_ai.pSetLayouts = &out->desc_set_layout;
    vkAllocateDescriptorSets(ctx->device, &ds_ai, &out->desc_set);
    return 0;
}

void vk_pipeline_destroy(VKContext *ctx, VKComputePipeline *pipe) {
    if (pipe->desc_set)
        vkFreeDescriptorSets(ctx->device, ctx->descriptor_pool, 1, &pipe->desc_set);
    if (pipe->pipeline) vkDestroyPipeline(ctx->device, pipe->pipeline, NULL);
    if (pipe->layout) vkDestroyPipelineLayout(ctx->device, pipe->layout, NULL);
    if (pipe->desc_set_layout) vkDestroyDescriptorSetLayout(ctx->device, pipe->desc_set_layout, NULL);
    memset(pipe, 0, sizeof(*pipe));
}

void vk_pipeline_bind_image(VKContext *ctx, VKComputePipeline *pipe, int binding, VKImage *img) {
    VkDescriptorImageInfo img_info;
    img_info.sampler = VK_NULL_HANDLE;
    img_info.imageView = img->view;
    img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write;
    memset(&write, 0, sizeof(write));
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = pipe->desc_set;
    write.dstBinding = (uint32_t)binding;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &img_info;
    vkUpdateDescriptorSets(ctx->device, 1, &write, 0, NULL);
}

void vk_pipeline_dispatch(VkCommandBuffer cmd, VKComputePipeline *pipe,
                           const void *push_data, size_t push_bytes,
                           uint32_t groups_x, uint32_t groups_y) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipe->layout, 0, 1, &pipe->desc_set, 0, NULL);
    if (push_data && push_bytes > 0)
        vkCmdPushConstants(cmd, pipe->layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, (uint32_t)push_bytes, push_data);
    vkCmdDispatch(cmd, groups_x, groups_y, 1);
}