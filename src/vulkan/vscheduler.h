#pragma once

#include "vulkan/vstructs.h"

namespace VSTIR {

    class VScheduler {
    public:
        VScheduler() {};
        ~VScheduler() {};
    public:
        void Initialize();
        VulkanCommands& Commands() { return m_Commands; }
        VkQueue& Queue() { return m_Queue; }
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
