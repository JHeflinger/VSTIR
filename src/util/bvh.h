#pragma once

#include "vulkan/vstructs.h"

namespace VSTIR {

    class BVH {
    public:
        static std::vector<NodeBVH> Create(const std::vector<Triangle>& triangles, const std::vector<glm::vec4>& vertices);
    };

}
