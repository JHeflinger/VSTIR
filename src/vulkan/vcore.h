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
        void Reconstruct();
        VGeneral& General() { return m_General; }
        VContext& Context() { return m_Context; }
        VScheduler& Scheduler() { return m_Scheduler; }
        std::vector<VulkanShader>& Shaders() { return m_Shaders; }
        VulkanGeometry& Geometry() { return m_Geometry; }
    private:
        void InitializeShaders();
        void InitializeGeometry();
        void InitializeGeometryBuffer(size_t size, size_t esize, VulkanDataBuffer* buffer, void* start);
    private:
        VGeneral m_General;
        VScheduler m_Scheduler;
        VContext m_Context;
        std::vector<VulkanShader> m_Shaders;
        VulkanGeometry m_Geometry;
    };

}
