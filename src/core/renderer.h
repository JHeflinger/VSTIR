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
        void LoadScene(std::string filepath);
    public:
        Backend& GetBackend() { return m_Backend; }
        CPUSwap& Swapchain() { return m_Swapchain; }
        Geometry& GetGeometry() { return m_Geometry; }
    private:
        void RecordCommand(uint32_t imageIndex);
        bool ConstructOBJ(const StateOBJ state);
    private:
        CPUSwap m_Swapchain;
        Backend m_Backend;
        Geometry m_Geometry;
        std::vector<NodeBVH> m_BVH;
    };

}
