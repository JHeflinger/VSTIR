#include "vutil.h"
#include "core/editor.h"
#include "core/get.h"
#include "util/log.h"
#include <memory>
#include <cstring>
#include <vulkan/vulkan.h>

namespace VSTIR {

    bool VUTILS::CheckValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        VkLayerProperties* availableLayers = (VkLayerProperties*)calloc(layerCount, sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);
        for (size_t i = 0; i < _metadata.Validation().size(); i++) {
            bool layerFound = false;
            for (size_t j = 0; j < layerCount; j++) {
                if (strcmp(_metadata.Validation()[i].c_str(), availableLayers[j].layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) {
                free(availableLayers);
                return false;
            }
        }
        free(availableLayers);
        return true;
    }

    VulkanFamilyGroup VUTILS::FindQueueFamilies(VkPhysicalDevice gpu) {
        VulkanFamilyGroup group{};
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
        VkQueueFamilyProperties* families = (VkQueueFamilyProperties*)calloc(count, sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, families);
        for (uint32_t i = 0; i < count; i++) {
            if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                group.graphics = (Schrodingnum){ i, true };
            }
            if ((families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
               !(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
               !(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                group.transfer = (Schrodingnum){ i, true };
            }
        }
        if (!group.transfer.exists) group.transfer = group.graphics;
        free(families);
        return group;
    }

    bool VUTILS::CheckGPUExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        VkExtensionProperties* availableExtensions = (VkExtensionProperties*)calloc(extensionCount, sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions);
        for (size_t i = 0; i < _metadata.Extensions().device.size(); i++) {
            bool extensionFound = false;
            for (size_t j = 0; j < extensionCount; j++) {
                if (strcmp(_metadata.Extensions().device[i].c_str(), availableExtensions[j].extensionName) == 0) {
                    extensionFound = true;
                    break;
                }
            }
            if (!extensionFound) {
                free(availableExtensions);
                return false;
            }
        }
        free(availableExtensions);
        return true;
    }

    void VUTILS::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VulkanDataBuffer* buffer) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkResult result = vkCreateBuffer(_interface, &bufferInfo, nullptr, &(buffer->buffer));
        ASSERT(result == VK_SUCCESS, "Unable to create buffer");
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(_interface, buffer->buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        Schrodingnum memoryType = FindMemoryType(memRequirements.memoryTypeBits, properties);
        ASSERT(memoryType.exists, "Unable to find memory for vertex buffer");
        allocInfo.memoryTypeIndex = memoryType.value;
        result = vkAllocateMemory(_interface, &allocInfo, nullptr, &(buffer->memory));
        ASSERT(result == VK_SUCCESS, "Unable to allocate memory for buffer");
        vkBindBufferMemory(_interface, buffer->buffer, buffer->memory, 0);
    }

    Schrodingnum VUTILS::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        Schrodingnum result{};
        VkPhysicalDeviceMemoryProperties memProperties{};
        vkGetPhysicalDeviceMemoryProperties(_gpu, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                result.value = i;
                result.exists = true;
                break;
            }
        }
        return result;
    }

    VkShaderModule VUTILS::CreateShader(SimpleFile* file) {
        VkShaderModuleCreateInfo createInfo{};
    	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    	createInfo.codeSize = file->size;
    	createInfo.pCode = (const uint32_t*)(file->data);
    	VkShaderModule shader;
    	VkResult result = vkCreateShaderModule(_interface, &createInfo, nullptr, &shader);
    	ASSERT(result == VK_SUCCESS, "Failed to create shader module");
    	return shader;
    }

    void VUTILS::CreateImage(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        VkSampleCountFlagBits numSamples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImageAspectFlags aspectFlags,
        VulkanImage* image) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = numSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkResult result = vkCreateImage(_interface, &imageInfo, nullptr, &(image->image));
        ASSERT(result == VK_SUCCESS, "Failed to create image!");

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(_interface, image->image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        Schrodingnum memoryType = FindMemoryType(memRequirements.memoryTypeBits, properties);
        ASSERT(memoryType.exists, "Unable to find valid memory type");
        allocInfo.memoryTypeIndex = memoryType.value;
        result = vkAllocateMemory(_interface, &allocInfo, nullptr, &(image->memory));
        ASSERT(result == VK_SUCCESS, "Failed to allocate image memory!");

        vkBindImageMemory(_interface, image->image, image->memory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image->image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = vkCreateImageView(_interface, &viewInfo, nullptr, &(image->view));
        ASSERT(result == VK_SUCCESS, "failed to create texture image view!");
    }

    VkCommandBuffer VUTILS::BeginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = _scheduler.Commands().pool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(_interface, &allocInfo, &commandBuffer);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    void VUTILS::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(_scheduler.Queue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(_scheduler.Queue());
        vkFreeCommandBuffers(_interface, _scheduler.Commands().pool, 1, &commandBuffer);
    }

    void VUTILS::TransitionImageLayout(
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        uint32_t mipLevels) {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else {
            FATAL("Unsupported layout transition!");
        }
        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        EndSingleTimeCommands(commandBuffer);
    }

    void VUTILS::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        EndSingleTimeCommands(commandBuffer);
    }

    void VUTILS::RecordGeneralBarrier(VkCommandBuffer command) {
        VkMemoryBarrier memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(
            command,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    }

}
