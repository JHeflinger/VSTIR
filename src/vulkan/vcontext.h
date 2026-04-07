#pragma once

#include "vulkan/vstructs.h"

namespace VSTIR {

    class VContext {
    public:
        VContext() {};
        ~VContext() {};
    public:
        void Initialize();
        VulkanImage& Target() { return m_Target; }
    private:
        void InitializePipeline();
        void InitializeTarget();
    private:
        VulkanPipeline m_Pipeline;
        VulkanImage m_Target;
    };

}
