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
        static void CreateImage(
            uint32_t width,
            uint32_t height,
            uint32_t mipLevels,
            VkSampleCountFlagBits numSamples,
            VkFormat format,
            VkImageTiling tiling,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkImageAspectFlags aspectFlags,
            VulkanImage* image);
        static VkCommandBuffer BeginSingleTimeCommands();
        static void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
        static void TransitionImageLayout(
            VkImage image,
            VkFormat format,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            uint32_t mipLevels);
        static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        static void RecordGeneralBarrier(VkCommandBuffer command);
        static void DestroyBuffer(VulkanDataBuffer buffer);
        static void CopyHostToBuffer(void* hostdata, size_t size, VkDeviceSize buffersize, VkBuffer buffer);
    };

}
