#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace VSTIR {

    struct RayGenerator {
        alignas(16) glm::vec3 accumulation;
        alignas(16) glm::vec3 wpos;
        alignas(16) glm::vec3 wnorm;
        alignas(4) uint32_t matid;
        alignas(4) uint32_t hitid;
        alignas(16) glm::vec3 ypos;
        alignas(16) glm::vec3 ynorm;
        alignas(16) glm::vec3 yradiance;
        alignas(4) float W;
        alignas(4) float wsum;
        alignas(4) uint32_t M;
        alignas(16) glm::vec3 pypos;
        alignas(16) glm::vec3 pynorm;
        alignas(16) glm::vec3 pyradiance;
        alignas(4) float pW;
        alignas(4) float pwsum;
        alignas(4) uint32_t pM;
        alignas(4) float pdepth;
        alignas(16) glm::vec3 pnorm;
    };

    typedef enum {
	    UNIFORM_BUFFER = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    	STORAGE_BUFFER = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    	STORAGE_IMAGE = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    } VulkanVariableType;

    struct CPUSwap {
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
        VkSemaphore imageAvailable;
        std::vector<VkSemaphore> renderFinished;
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
        alignas(4) uint32_t seed;
        alignas(4) uint32_t samples;
        alignas(4) float fov;
        alignas(4) float width;
        alignas(4) float height;
        alignas(16) glm::mat4 previousvpm;
        alignas(4) float depththreshold;
        alignas(4) float normalthreshold;
        alignas(4) uint32_t contributioncap;
        alignas(4) uint32_t candidatecap;
        alignas(4) uint32_t spacerange;
        alignas(4) uint32_t spacecount;
    };

    struct VulkanSwapchain {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> images;
        std::vector<VkImageView> views;
        VkFormat format;
        VkExtent2D extent;
    };

    struct Triangle {
        alignas(4) uint32_t a;
        alignas(4) uint32_t b;
        alignas(4) uint32_t c;
        alignas(4) uint32_t an;
        alignas(4) uint32_t bn;
        alignas(4) uint32_t cn;
        alignas(4) uint32_t material;
    };

    struct Material {
        alignas(16) glm::vec3 emission;
        alignas(16) glm::vec3 ambient;
        alignas(16) glm::vec3 diffuse;
        alignas(16) glm::vec3 specular;
        alignas(16) glm::vec3 absorbtion;
        alignas(16) glm::vec3 dispersion;
        alignas(4) float ior;
        alignas(4) float shiny;
        alignas(4) uint32_t model;
    };

    #define BVH_LEAF 0
    #define BVH_LEFT_ONLY 1
    #define BVH_RIGHT_ONLY 2
    #define BVH_BOTH 3

    struct NodeBVH {
        alignas(16) glm::vec3 min;
        alignas(16) glm::vec3 max;
        alignas(4) uint32_t config;
        alignas(4) uint32_t left;
        alignas(4) uint32_t right;
    };

    struct AABB {
        glm::vec3 min;
        glm::vec3 max;
        glm::vec3 centroid;
    };

    struct Geometry {
        std::vector<NodeBVH> bvh;
        std::vector<glm::vec4> vertices;
        std::vector<glm::vec4> normals;
        std::vector<Triangle> triangles;
        std::vector<uint32_t> emissives;
        std::vector<Material> materials;
        size_t bvh_size;
        size_t vertices_size;
        size_t normals_size;
        size_t triangles_size;
        size_t emissives_size;
        size_t materials_size;
        size_t raygen_size;
    };

    struct VulkanGeometry {
        VulkanDataBuffer bvh;
        VulkanDataBuffer normals;
        VulkanDataBuffer vertices;
        VulkanDataBuffer triangles;
        VulkanDataBuffer emissives;
        VulkanDataBuffer materials;
    };

    struct Face {
        uint32_t a;
        uint32_t b;
        uint32_t c;
        uint32_t at;
        uint32_t bt;
        uint32_t ct;
        uint32_t an;
        uint32_t bn;
        uint32_t cn;
        bool textures;
        bool normals;
    };

    struct UseMaterialMarker {
        uint32_t faceIndex;
        uint32_t materialIndex;
    };

    struct StateOBJ {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<Face> faces;
        std::vector<Material> materials;
        std::vector<std::string> mnames;
        std::vector<UseMaterialMarker> markers;
        std::string filepath;
    };

    // struct Camera {
    //     glm::vec3 position;
    //     glm::vec3 look;
    //     glm::vec3 up;
    // 	float fov;
    // };

}
