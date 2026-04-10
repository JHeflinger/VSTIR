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
        VulkanImage& Target() { return m_Target; }
        VData& Data() { return m_Data; }
        VulkanPipeline& Pipeline() { return m_Pipeline; }
        VulkanSwapchain& Swapchain() { return m_Swapchain; }
    private:
        void InitializePipeline();
        void InitializeTarget();
        void InitializeSwapchain();
    private:
        VulkanSwapchain m_Swapchain;
        VulkanPipeline m_Pipeline;
        VulkanImage m_Target;
        VData m_Data;
    };

}
