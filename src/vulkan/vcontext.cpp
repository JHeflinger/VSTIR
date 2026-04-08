#include "vcontext.h"
#include "util/log.h"
#include "core/get.h"
#include "util/file.h"
#include "vulkan/vutil.h"

namespace VSTIR {

    void VContext::Initialize() {
        InitializeTarget();
        m_Data.Initialize();
        InitializePipeline();
    }

    void VContext::InitializePipeline() {
        size_t num_shaders = _shaders.size();
        m_Pipeline.pipeline = (VkPipeline*)calloc(num_shaders, sizeof(VkPipeline));
        m_Pipeline.layout = (VkPipelineLayout*)calloc(num_shaders, sizeof(VkPipelineLayout));
        VkShaderModule* shadermodules = (VkShaderModule*)calloc(num_shaders, sizeof(VkShaderModule));
        VkComputePipelineCreateInfo* pipelineInfos = (VkComputePipelineCreateInfo*)calloc(num_shaders, sizeof(VkComputePipelineCreateInfo));
        for (size_t i = 0; i < num_shaders; i++) {
            VulkanShader& shader = _shaders[i];
            SimpleFile* shadercode = VFILE::ReadFile(shader.filename.c_str());
            shadermodules[i] = VUTILS::CreateShader(shadercode);
            VFILE::FreeFile(shadercode);

            VkPipelineShaderStageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            createInfo.module = shadermodules[i];
            createInfo.pName = "main";

            VkPipelineLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &(m_Data.Descriptors()[i].layout);
            layoutInfo.pushConstantRangeCount = 0;

            VkResult result = vkCreatePipelineLayout(_interface, &(layoutInfo), nullptr, &(m_Pipeline.layout[i]));
            if (result != VK_SUCCESS) FATAL("Failed to create pipeline layout!");

            pipelineInfos[i].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineInfos[i].layout = m_Pipeline.layout[i];
            pipelineInfos[i].stage = createInfo;
        }

        VkResult result = vkCreateComputePipelines(
            _interface, VK_NULL_HANDLE, num_shaders,
            pipelineInfos, nullptr, m_Pipeline.pipeline);
        if (result != VK_SUCCESS) FATAL("Failed to create pipeline!");

        for (size_t i = 0; i < num_shaders; i++) vkDestroyShaderModule(_interface, shadermodules[i], nullptr);
        free(shadermodules);
        free(pipelineInfos);
    }

    void VContext::InitializeTarget() {
        VUTILS::CreateImage(
            _width,
            _height,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            &(m_Target));
        VUTILS::TransitionImageLayout(
            m_Target.image,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            1);
    }

}
