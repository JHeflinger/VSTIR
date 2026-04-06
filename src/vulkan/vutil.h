#pragma once

#include "vulkan/vulkan.h"
#include "vulkan/vstructs.h"

namespace VSTIR {

    class VUTILS {
    public:
        static bool CheckValidationLayerSupport();
        static VulkanFamilyGroup FindQueueFamilies(VkPhysicalDevice gpu);
        static bool CheckGPUExtensionSupport(VkPhysicalDevice device);
    };

}
