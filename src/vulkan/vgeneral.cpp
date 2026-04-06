#include "vgeneral.h"
#include "vulkan/vutil.h"
#include "core/get.h"
#include "util/log.h"

VKAPI_ATTR VkBool32 VKAPI_CALL _VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    ASSERT(pUserData == nullptr, "User data has not been set up to be handled");
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        WARN("%s[VULKAN] [%s]%s %s",
            LOGCOLOR_YELLOW,
            (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT ? "GENERAL" :
                (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "VALIDATION" : "PERFORMANCE")),
            LOGCOLOR_RESET,
            pCallbackData->pMessage);
    } else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        printf("%s[FATAL] [VULKAN] [%s]%s %s",
            LOGCOLOR_RED,
            (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT ? "GENERAL" :
                (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "VALIDATION" : "PERFORMANCE")),
            LOGCOLOR_RESET,
            pCallbackData->pMessage);
		exit(1);
    }
    return VK_FALSE;
}

namespace VSTIR {

    VGeneral::VGeneral() {

    }

    VGeneral::~VGeneral() {

    }

    void VGeneral::Initialize() {
        if (!VUTILS::CheckValidationLayerSupport()) FATAL("Requested validation layers are not available");
        std::vector<char*> required_extensions;
        for (auto& s : _metadata.Extensions().required) required_extensions.push_back(s.data());
        std::vector<char*> validation_extensions;
        for (auto& s : _metadata.Validation()) validation_extensions.push_back(s.data());
        std::vector<char*> device_extensions;
        for (auto& s : _metadata.Extensions().device) device_extensions.push_back(s.data());
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "VSTIR Renderer";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "VSTIR Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = (uint32_t)(_metadata.Extensions().required.size());
        createInfo.ppEnabledExtensionNames = required_extensions.data();
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        createInfo.enabledLayerCount = _metadata.Validation().size();
        createInfo.ppEnabledLayerNames = validation_extensions.data();
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = _VulkanDebugCallback;
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        VkResult result = vkCreateInstance(&createInfo, nullptr, &(m_Instance));
        if (result != VK_SUCCESS) FATAL("Failed to create vulkan instance");
    	VkDebugUtilsMessengerCreateInfoEXT dcreateInfo{};
    	dcreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    	dcreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    	dcreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    	dcreateInfo.pfnUserCallback = _VulkanDebugCallback;
    	dcreateInfo.pUserData = nullptr;
    	PFN_vkCreateDebugUtilsMessengerEXT messenger_extension = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT");
    	if (messenger_extension == nullptr) FATAL("Failed to set up debug messenger");
    	messenger_extension(m_Instance, &dcreateInfo, nullptr, &(m_Messenger));
    	uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
        if (deviceCount == 0) FATAL("No devices with vulkan support were found");
        VkPhysicalDevice* devices = (VkPhysicalDevice*)calloc(deviceCount, sizeof(VkPhysicalDevice));
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices);
        uint32_t score = 0;
        uint32_t ind = 0;
        for (uint32_t i = 0; i < deviceCount; i++) {
            VulkanFamilyGroup families = VUTILS::FindQueueFamilies(devices[i]);
            if (!families.graphics.exists) continue;
            if (!VUTILS::CheckGPUExtensionSupport(devices[i])) continue;
            uint32_t curr_score = 0;
            VkPhysicalDeviceProperties deviceProperties;
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
            vkGetPhysicalDeviceFeatures(devices[i], &deviceFeatures);
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) curr_score += 10000;
            curr_score += deviceProperties.limits.maxImageDimension2D;
            if (!deviceFeatures.geometryShader) curr_score = 0;
            if (!deviceFeatures.samplerAnisotropy) curr_score = 0;
            if (curr_score > score) {
                score = curr_score;
                ind = i;
            }
        }
        if (score == 0) FATAL("A suitable GPU could not be found");
        VkPhysicalDeviceProperties dp;
        vkGetPhysicalDeviceProperties(devices[ind], &dp);
        m_GPUName = std::string(dp.deviceName);
        m_GPU = devices[ind];
        free(devices);
    	VulkanFamilyGroup families = VUTILS::FindQueueFamilies(m_GPU);
        VkDeviceQueueCreateInfo queueInfos[2];
        float priority = 1.0f;
        uint32_t queueCount = 1;
        queueInfos[0] = (VkDeviceQueueCreateInfo){};
        queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[0].queueFamilyIndex = families.graphics.value;
        queueInfos[0].queueCount = 1;
        queueInfos[0].pQueuePriorities = &priority;
        if (families.transfer.value != families.graphics.value) {
            queueInfos[1] = (VkDeviceQueueCreateInfo){};
            queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfos[1].queueFamilyIndex = families.transfer.value;
            queueInfos[1].queueCount = 1;
            queueInfos[1].pQueuePriorities = &priority;
            queueCount = 2;
        }
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.sampleRateShading = VK_TRUE;
        VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures{};
        timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        timelineFeatures.timelineSemaphore = VK_TRUE;
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &timelineFeatures;
        deviceCreateInfo.pQueueCreateInfos = queueInfos;
        deviceCreateInfo.queueCreateInfoCount = queueCount;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        deviceCreateInfo.enabledExtensionCount = _metadata.Extensions().device.size();
        deviceCreateInfo.ppEnabledExtensionNames = device_extensions.data();
        deviceCreateInfo.enabledLayerCount = _metadata.Validation().size();
        deviceCreateInfo.ppEnabledLayerNames = validation_extensions.data();
        result = vkCreateDevice(m_GPU, &deviceCreateInfo, nullptr, &(m_Interface));
    	if (result != VK_SUCCESS) FATAL("Failed to create logical device");
    }

}
