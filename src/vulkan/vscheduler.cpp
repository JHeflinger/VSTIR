#include "vscheduler.h"
#include "vulkan/vutil.h"
#include "core/get.h"
#include "util/log.h"

namespace VSTIR {

    void VScheduler::Initialize() {
        InitializeSyncro();
        InitializeCommands();
        InitializeQueue();
    }

    void VScheduler::InitializeSyncro() {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkResult result = vkCreateFence(_interface, &fenceInfo, nullptr, &(m_Syncro.fence));
        if (result != VK_SUCCESS) FATAL("Failed to create fence");
        VkSemaphoreCreateInfo sInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        vkCreateSemaphore(_interface, &sInfo, nullptr, &m_Syncro.imageAvailable);
        vkCreateSemaphore(_interface, &sInfo, nullptr, &m_Syncro.renderFinished);
    }

    void VScheduler::InitializeCommands() {
        VulkanFamilyGroup queueFamilyIndices = VUTILS::FindQueueFamilies(_gpu);
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphics.value;
        VkResult result = vkCreateCommandPool(_interface, &poolInfo, nullptr, &(m_Commands.pool));
        if (result != VK_SUCCESS) FATAL("Failed to create command pool!");
    	VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_Commands.pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        result = vkAllocateCommandBuffers(_interface, &allocInfo, &(m_Commands.command));
        if (result != VK_SUCCESS) FATAL("Failed to create command buffer");
    }

    void VScheduler::InitializeQueue() {
	    VulkanFamilyGroup families = VUTILS::FindQueueFamilies(_gpu);
        vkGetDeviceQueue(_interface, families.graphics.value, 0, &m_Queue);
    }

}
