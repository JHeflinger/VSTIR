#pragma once

#define GLFW_INCLUDE_VULKAN

#include <cstdint>
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace VSTIR {

    class Editor;

    class VCore {
    public:
        VCore(Editor* editor);
        ~VCore();
    public:
        void Render();
        void Wait();
    private:
        void InitVulkan();
        void InitInstance();
        void InitSurface();
        void InitPhysicalDevice();
        void InitLogicalDevice();
        void InitSwapchain();
        void InitImageViews();
        void InitRenderPass();
        void InitFrameBuffers();
        void InitCommandPool();
        void InitCommandbuffer();
        void InitSyncObjects();
    private:
        Editor* m_Editor;
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        VkQueue m_PresentQueue = VK_NULL_HANDLE;
        uint32_t m_GraphicsFamily = 0;
        uint32_t m_PresentFamily = 0;
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        VkFormat m_SwapFormat = {};
        VkExtent2D m_SwapExtent = {};
        std::vector<VkImage> m_SwapImages;
        std::vector<VkImageView> m_SwapViews;
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_Framebuffers;
        VkCommandPool m_CmdPool = VK_NULL_HANDLE;
        VkCommandBuffer m_CmdBuf = VK_NULL_HANDLE;
        std::vector<VkSemaphore> m_ImageReady;
        std::vector<VkSemaphore> m_RenderDone;
        std::vector<VkFence> m_InFlight;
        uint32_t m_CurrentFrame = 0;
    };

}
