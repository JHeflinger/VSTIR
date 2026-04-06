#include "vulkan/vcore.h"
#include "core/editor.h"
#include "util/log.h"

namespace VSTIR {

    VCore::VCore(Editor* editor) : m_Editor(editor) {
        InitVulkan();
    }

    VCore::~VCore() {
        for (size_t i = 0; i < m_InFlight.size(); ++i) {
            vkDestroyFence(m_Device, m_InFlight[i], nullptr);
            vkDestroySemaphore(m_Device, m_RenderDone[i], nullptr);
            vkDestroySemaphore(m_Device, m_ImageReady[i], nullptr);
        }
        vkDestroyCommandPool(m_Device, m_CmdPool, nullptr);
        for (auto fb : m_Framebuffers) vkDestroyFramebuffer(m_Device, fb, nullptr);
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
        for (auto view : m_SwapViews) vkDestroyImageView  (m_Device, view, nullptr);
        vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
        vkDestroyDevice(m_Device, nullptr);
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        vkDestroyInstance(m_Instance, nullptr);
    }

    void VCore::Render() {
        uint32_t frame = m_CurrentFrame;
        vkWaitForFences(m_Device, 1, &m_InFlight[frame], VK_TRUE, UINT64_MAX);
        vkResetFences (m_Device, 1, &m_InFlight[frame]);
        uint32_t imageIndex = 0;
        vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_ImageReady[frame], VK_NULL_HANDLE, &imageIndex);
        vkResetCommandBuffer(m_CmdBuf, 0);
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(m_CmdBuf, &begin);
        VkClearValue clearWhite{};
        clearWhite.color = {1.0f, 1.0f, 1.0f, 1.0f};
        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = m_RenderPass;
        rpBegin.framebuffer = m_Framebuffers[imageIndex];
        rpBegin.renderArea.offset = {0, 0};
        rpBegin.renderArea.extent = m_SwapExtent;
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues = &clearWhite;
        vkCmdBeginRenderPass(m_CmdBuf, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(m_CmdBuf);
        vkEndCommandBuffer(m_CmdBuf);
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &m_ImageReady[frame];
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &m_CmdBuf;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &m_RenderDone[frame];
        vkQueueSubmit(m_GraphicsQueue, 1, &submit, m_InFlight[frame]);
        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &m_RenderDone[frame];
        present.swapchainCount = 1;
        present.pSwapchains = &m_Swapchain;
        present.pImageIndices = &imageIndex;
        vkQueuePresentKHR(m_PresentQueue, &present);
        m_CurrentFrame = (m_CurrentFrame + 1) % m_SwapImages.size();
    }

    void VCore::Wait() {
        vkDeviceWaitIdle(m_Device);
    }

    void VCore::InitVulkan() {
        InitInstance();
        InitSurface();
        InitPhysicalDevice();
        InitLogicalDevice();
        InitSwapchain();
        InitImageViews();
        InitRenderPass();
        InitFrameBuffers();
        InitCommandPool();
        InitCommandbuffer();
        InitSyncObjects();
    }

    void VCore::InitInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "VSTIR";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;
        uint32_t glfwCount = 0;
        const char** glfwExts  = glfwGetRequiredInstanceExtensions(&glfwCount);
        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &appInfo;
        ci.enabledExtensionCount = glfwCount;
        ci.ppEnabledExtensionNames = glfwExts;
        const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = layers;
        if (vkCreateInstance(&ci, nullptr, &m_Instance) != VK_SUCCESS) FATAL("vkCreateInstance failed");
    }

    void VCore::InitSurface() {
        if (glfwCreateWindowSurface(m_Instance, m_Editor->Window(), nullptr, &m_Surface) != VK_SUCCESS)
            FATAL("glfwCreateWindowSurface failed");
    }

    void VCore::InitPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
        if (count == 0) FATAL("No Vulkan GPUs found");
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());
        for (auto& dev : devices) {
            uint32_t qCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
            std::vector<VkQueueFamilyProperties> props(qCount);
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, props.data());
            bool foundGraphics = false, foundPresent = false;
            for (uint32_t i = 0; i < qCount; ++i) {
                if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    m_GraphicsFamily = i;
                    foundGraphics = true;
                }
                VkBool32 present = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_Surface, &present);
                if (present) {
                    m_PresentFamily = i;
                    foundPresent = true;
                }
            }
            if (foundGraphics && foundPresent) {
                m_PhysicalDevice = dev;
                return;
            }
        }
        FATAL("No suitable GPU found");
    }

    void VCore::InitLogicalDevice() {
        std::vector<VkDeviceQueueCreateInfo> queueCIs;
        float priority = 1.0f;
        VkDeviceQueueCreateInfo gq{};
        gq.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        gq.queueFamilyIndex = m_GraphicsFamily;
        gq.queueCount = 1;
        gq.pQueuePriorities = &priority;
        queueCIs.push_back(gq);
        if (m_PresentFamily != m_GraphicsFamily) {
            VkDeviceQueueCreateInfo pq = gq;
            pq.queueFamilyIndex = m_PresentFamily;
            queueCIs.push_back(pq);
        }
        const char* deviceExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        ci.queueCreateInfoCount = (uint32_t)queueCIs.size();
        ci.pQueueCreateInfos = queueCIs.data();
        ci.enabledExtensionCount = 1;
        ci.ppEnabledExtensionNames = deviceExts;
        if (vkCreateDevice(m_PhysicalDevice, &ci, nullptr, &m_Device) != VK_SUCCESS)
            FATAL("vkCreateDevice failed");
        vkGetDeviceQueue(m_Device, m_GraphicsFamily, 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_PresentFamily,  0, &m_PresentQueue);
    }

    void VCore::InitSwapchain() {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &caps);
        m_SwapExtent = caps.currentExtent;
        if (m_SwapExtent.width == UINT32_MAX) {
            m_SwapExtent.width  = (uint32_t)m_Editor->Width();
            m_SwapExtent.height = (uint32_t)m_Editor->Height();
        }
        uint32_t fmtCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &fmtCount, formats.data());
        m_SwapFormat = formats[0].format;
        for (auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                m_SwapFormat = f.format;
                break;
            }
        }
        VkSwapchainCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface = m_Surface;
        ci.minImageCount = caps.minImageCount + 1;
        ci.imageFormat = m_SwapFormat;
        ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        ci.imageExtent = m_SwapExtent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        ci.preTransform = caps.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        ci.clipped = VK_TRUE;
        uint32_t families[] = {m_GraphicsFamily, m_PresentFamily};
        if (m_GraphicsFamily != m_PresentFamily) {
            ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices = families;
        } else {
            ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        if (vkCreateSwapchainKHR(m_Device, &ci, nullptr, &m_Swapchain) != VK_SUCCESS)
            FATAL("vkCreateSwapchainKHR failed");
        uint32_t imgCount = 0;
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imgCount, nullptr);
        m_SwapImages.resize(imgCount);
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imgCount, m_SwapImages.data());
    }

    void VCore::InitImageViews() {
        m_SwapViews.resize(m_SwapImages.size());
        for (size_t i = 0; i < m_SwapImages.size(); ++i) {
            VkImageViewCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ci.image = m_SwapImages[i];
            ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ci.format = m_SwapFormat;
            ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ci.subresourceRange.baseMipLevel = 0;
            ci.subresourceRange.levelCount = 1;
            ci.subresourceRange.baseArrayLayer = 0;
            ci.subresourceRange.layerCount = 1;
            if (vkCreateImageView(m_Device, &ci, nullptr, &m_SwapViews[i]) != VK_SUCCESS)
                FATAL("vkCreateImageView failed");
        }
    }

    void VCore::InitRenderPass() {
        VkAttachmentDescription color{};
        color.format = m_SwapFormat;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        ci.attachmentCount = 1;
        ci.pAttachments = &color;
        ci.subpassCount = 1;
        ci.pSubpasses = &subpass;
        ci.dependencyCount = 1;
        ci.pDependencies = &dep;
        if (vkCreateRenderPass(m_Device, &ci, nullptr, &m_RenderPass) != VK_SUCCESS)
            FATAL("vkCreateRenderPass failed");
    }

    void VCore::InitFrameBuffers() {
        m_Framebuffers.resize(m_SwapViews.size());
        for (size_t i = 0; i < m_SwapViews.size(); ++i) {
            VkFramebufferCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            ci.renderPass = m_RenderPass;
            ci.attachmentCount = 1;
            ci.pAttachments = &m_SwapViews[i];
            ci.width = m_SwapExtent.width;
            ci.height = m_SwapExtent.height;
            ci.layers = 1;
            if (vkCreateFramebuffer(m_Device, &ci, nullptr, &m_Framebuffers[i]) != VK_SUCCESS)
                FATAL("vkCreateFramebuffer failed");
        }
    }

    void VCore::InitCommandPool() {
        VkCommandPoolCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = m_GraphicsFamily;
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(m_Device, &ci, nullptr, &m_CmdPool) != VK_SUCCESS)
            FATAL("vkCreateCommandPool failed");
    }

    void VCore::InitCommandbuffer() {
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = m_CmdPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(m_Device, &ai, &m_CmdBuf) != VK_SUCCESS)
            FATAL("vkAllocateCommandBuffers failed");
    }

    void VCore::InitSyncObjects() {
        size_t count = m_SwapImages.size();
        m_ImageReady.resize(count);
        m_RenderDone.resize(count);
        m_InFlight.resize(count);
        VkSemaphoreCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (size_t i = 0; i < count; ++i) {
            if (vkCreateSemaphore(m_Device, &si, nullptr, &m_ImageReady[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_Device, &si, nullptr, &m_RenderDone[i]) != VK_SUCCESS ||
                vkCreateFence    (m_Device, &fi, nullptr, &m_InFlight[i])   != VK_SUCCESS)
                FATAL("Sync object creation failed");
        }
    }

}
