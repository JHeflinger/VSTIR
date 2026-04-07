#pragma once

#include "vulkan/vgeneral.h"
#include "vulkan/vscheduler.h"
#include "vulkan/vcontext.h"

namespace VSTIR {

    class VCore {
    public:
        VCore() {};
        ~VCore() {};
    public:
        void Initialize();
        VGeneral& General() { return m_General; }
        VContext& Context() { return m_Context; }
        std::vector<VulkanShader>& Shaders() { return m_Shaders; }
    private:
        void InitializeBridge();
        void InitializeShaders();
    private:
        VGeneral m_General;
        VScheduler m_Scheduler;
        VulkanDataBuffer m_Bridge;
        VContext m_Context;
        std::vector<VulkanShader> m_Shaders;
    };

}
