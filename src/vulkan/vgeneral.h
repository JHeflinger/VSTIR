#pragma once

#include "vulkan/vulkan.h"
#include <string>

namespace VSTIR {

    class VGeneral {
    public:
        VGeneral() {};
        ~VGeneral() {};
    public:
        void Initialize();
        VkDevice Interface() { return m_Interface; }
        VkPhysicalDevice GPU() { return m_GPU; }
    private:
        VkDebugUtilsMessengerEXT m_Messenger;
        VkInstance m_Instance;
        VkPhysicalDevice m_GPU;
        VkDevice m_Interface;
	    std::string m_GPUName;
    };

}
