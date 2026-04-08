#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace VSTIR {

    struct RayGenerator {
        alignas(4) uint32_t tid; // dummy values for now - use what is needed for ReSTIR later
        alignas(4) float distance;
    };

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

    struct UBOArray {
        VulkanDataBuffer object;
        void* mapped;
    };

    struct VulkanDescriptors {
        VkDescriptorPool pool;
        VkDescriptorSet set;
        VkDescriptorSetLayout layout;
    };

    struct UniformBufferObject {
        alignas(16) glm::vec3 look;
        alignas(16) glm::vec3 position;
        alignas(16) glm::vec3 up;
        alignas(16) glm::vec3 u;
        alignas(16) glm::vec3 v;
        alignas(16) glm::vec3 w;
        alignas(4) uint32_t triangles;
        alignas(4) float fov;
        alignas(4) float width;
        alignas(4) float height;
    };

}
