#pragma once

#include "vulkan/vstructs.h"

namespace VSTIR {

    class VSHADERS {
    public:
        static VulkanShader GenerateShader(std::string filepath, std::string objpath);
    };

}
