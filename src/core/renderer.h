#pragma once

#include "vulkan/backend.h"
#include "vulkan/vstructs.h"
#include <cstddef>

namespace VSTIR {

    class Editor;

    class Renderer {
    public:
        Renderer() {};
        ~Renderer() {};
    public:
        void Initialize();
        void Render();
        Backend& GetBackend() { return m_Backend; }
        CPUSwap& Swapchain() { return m_Swapchain; }
    private:
        void RecordCommand();
    private:
        CPUSwap m_Swapchain;
        Backend m_Backend;
    };

}
