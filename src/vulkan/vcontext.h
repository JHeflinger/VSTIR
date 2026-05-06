#pragma once

#include "vulkan/vstructs.h"
#include "vulkan/vdata.h"

namespace VSTIR {

    class VContext {
    public:
        VContext() {};
        ~VContext() {};
    public:
        void Initialize();
        void Reconstruct();
        void ResizeSwapchain(uint32_t width, uint32_t height);
        void ResizeTarget();
        VulkanImage& Target() { return m_Target; }
        VulkanImage& RawTarget() { return m_RawTarget; }
        VulkanImage& ReSTIRTarget() { return m_ReSTIRTarget; }
        VData& Data() { return m_Data; }
        VulkanPipeline& Pipeline() { return m_Pipeline; }
        VulkanSwapchain& Swapchain() { return m_Swapchain; }
    private:
        void InitializePipeline();
        void InitializeTarget();
        void DestroyTarget();
        void InitializeSwapchain(uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
        void DestroySwapchain();
    private:
        VulkanSwapchain m_Swapchain;
        VulkanPipeline m_Pipeline;
        VulkanImage m_Target;
        VulkanImage m_RawTarget;
        VulkanImage m_ReSTIRTarget;
        VData m_Data;
    };

}
