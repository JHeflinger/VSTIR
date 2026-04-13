#include "vcontext.h"
#include "util/log.h"
#include "core/get.h"
#include "util/file.h"
#include "vulkan/vutil.h"

namespace VSTIR {

    void VContext::Initialize() {
        InitializeSwapchain((uint32_t)_width, (uint32_t)_height);
        InitializeTarget();
        m_Data.Initialize();
        InitializePipeline();
    }

    void VContext::Reconstruct() {
        m_Data.Reconstruct();
    }

    void VContext::ResizeSwapchain(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) {
            return;
        }
        VkSwapchainKHR oldSwapchain = m_Swapchain.swapchain;
        for (VkImageView view : m_Swapchain.views) {
            vkDestroyImageView(_interface, view, nullptr);
        }
        m_Swapchain.views.clear();
        m_Swapchain.images.clear();
        InitializeSwapchain(width, height, oldSwapchain);
        if (oldSwapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(_interface, oldSwapchain, nullptr);
        }
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

    void VContext::InitializeSwapchain(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain) {
        VkSurfaceKHR surface = _general.Surface();
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_gpu, surface, &caps);
        uint32_t fmtCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(_gpu, surface, &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(_gpu, surface, &fmtCount, formats.data());
        VkSurfaceFormatKHR chosen = formats[0];
        for (auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                { chosen = f; break; }

        VkSwapchainCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = std::min(caps.minImageCount + 1, caps.maxImageCount > 0 ? caps.maxImageCount : UINT32_MAX);
        info.imageFormat = chosen.format;
        info.imageColorSpace = chosen.colorSpace;
        info.imageExtent = { width, height };
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = oldSwapchain;

        m_Swapchain.format = chosen.format;
        m_Swapchain.extent = info.imageExtent;
        vkCreateSwapchainKHR(_interface, &info, nullptr, &m_Swapchain.swapchain);

        uint32_t imgCount; vkGetSwapchainImagesKHR(_interface, m_Swapchain.swapchain, &imgCount, nullptr);
        m_Swapchain.images.resize(imgCount);
        vkGetSwapchainImagesKHR(_interface, m_Swapchain.swapchain, &imgCount, m_Swapchain.images.data());
        m_Swapchain.views.resize(imgCount);
        for (uint32_t i = 0; i < imgCount; i++) {
            VkImageViewCreateInfo vinfo{};
            vinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vinfo.image = m_Swapchain.images[i];
            vinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vinfo.format = chosen.format;
            vinfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCreateImageView(_interface, &vinfo, nullptr, &m_Swapchain.views[i]);
        }
    }

    void VContext::DestroySwapchain() {
        for (VkImageView view : m_Swapchain.views) {
            vkDestroyImageView(_interface, view, nullptr);
        }
        m_Swapchain.views.clear();
        m_Swapchain.images.clear();
        if (m_Swapchain.swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(_interface, m_Swapchain.swapchain, nullptr);
            m_Swapchain.swapchain = VK_NULL_HANDLE;
        }
    }

}
