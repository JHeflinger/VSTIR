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

}
