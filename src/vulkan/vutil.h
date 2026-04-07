#pragma once

#include "vulkan/vulkan.h"
#include "vulkan/vstructs.h"
#include "util/file.h"

namespace VSTIR {

    class VUTILS {
    public:
        static bool CheckValidationLayerSupport();
        static VulkanFamilyGroup FindQueueFamilies(VkPhysicalDevice gpu);
        static bool CheckGPUExtensionSupport(VkPhysicalDevice device);
        static void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VulkanDataBuffer* buffer);
        static Schrodingnum FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        static VkShaderModule CreateShader(SimpleFile* file);
    };

}
