#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>

namespace VSTIR {

    struct VExtensionData {
        std::vector<std::string> required;
        std::vector<std::string> device;
    };

    struct Schrodingnum {
        uint32_t value;
        bool exists;
    };

    struct VulkanFamilyGroup {
        Schrodingnum graphics;
        Schrodingnum transfer;
    };

    struct VulkanSyncro {
        VkFence fence;
    };

    struct VulkanCommands {
        VkCommandPool pool;
        VkCommandBuffer command;
    };

}
