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
        VulkanImage& Target() { return m_Target; }
        VData& Data() { return m_Data; }
        VulkanPipeline& Pipeline() { return m_Pipeline; }
    private:
        void InitializePipeline();
        void InitializeTarget();
    private:
        VulkanPipeline m_Pipeline;
        VulkanImage m_Target;
        VData m_Data;
    };

}
