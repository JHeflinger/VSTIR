#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>

namespace VSTIR {

    typedef enum {
	    UNIFORM_BUFFER = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    	STORAGE_BUFFER = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    	STORAGE_IMAGE = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    } VulkanVariableType;

    struct CPUSwap {
	    // TODO: rendertargets - RenderTexture2D target[CPUSWAP_LENGTH];
	    size_t index;
        void* reference;
    };

    struct VExtensionData {
        std::vector<std::string> required;
        std::vector<std::string> device;
    };

    struct Schrodingnum {
        uint32_t value;
        bool exists;
    };

    struct SchrodingRef {
	    bool reference;
	    void* value;
    };

    struct SchrodingSize {
	    SchrodingRef count;
        float reduction;
	    size_t size;
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

    struct VulkanDataBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
    };

    struct VulkanPipeline {
        VkPipeline* pipeline;
        VkPipelineLayout* layout;
    };

    struct VulkanImage {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
    };

    struct VulkanBoundVariable {
        VulkanVariableType type;
        SchrodingRef data;
        SchrodingSize size;
    };

    struct VulkanShader {
        std::string filename;
        std::vector<VulkanBoundVariable> variables;
    };

}
