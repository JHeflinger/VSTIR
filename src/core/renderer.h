#pragma once

#include "vulkan/backend.h"
#include <cstddef>

namespace VSTIR {

    class Editor;

    class Renderer {
    public:
        Renderer() {};
        ~Renderer() {};
    public:
        void Initialize();
        Backend& GetBackend() { return m_Backend; }
    private:
        Backend m_Backend;
    };

}
