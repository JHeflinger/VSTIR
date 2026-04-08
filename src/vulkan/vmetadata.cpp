#include "vmetadata.h"
#include "vulkan/vulkan.h"
#include <GLFW/glfw3.h>

namespace VSTIR {

    void VMetadata::Initialize() {
        m_Validation.push_back("VK_LAYER_KHRONOS_validation");
        m_Extensions.required.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        m_Extensions.device.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        uint32_t glfwCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwCount);
        for (uint32_t i = 0; i < glfwCount; i++) m_Extensions.required.push_back(glfwExts[i]);
    }

}
