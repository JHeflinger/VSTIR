#pragma once

#include "vulkan/vstructs.h"

namespace VSTIR {

    class VScheduler {
    public:
        VScheduler() {};
        ~VScheduler() {};
    public:
        void Initialize();
        void RecreateRenderFinishedSemaphores(uint32_t imageCount);
        VulkanCommands& Commands() { return m_Commands; }
        VkQueue& Queue() { return m_Queue; }
        VulkanSyncro& Syncro() { return m_Syncro; }
    private:
        void InitializeSyncro();
        void InitializeCommands();
        void InitializeQueue();
    private:
        VulkanSyncro m_Syncro;
        VulkanCommands m_Commands;
        VkQueue m_Queue;
    };

}
