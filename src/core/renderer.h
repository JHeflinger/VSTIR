#pragma once

#include "vulkan/backend.h"
#include "vulkan/vstructs.h"
#include <cstddef>

#include "camera.h"

namespace VSTIR {

    class Editor;

    struct RenderSettings {
        bool accumulate_samples = false;
        int32_t sample_count = 1;

        float resolution_scale = 1.0f;
        uint32_t _last_render_width  = 0; // internal: used to detect render-size changes
        uint32_t _last_render_height = 0;

        bool denoiser = false; // TODO implement
        bool restir = false;

        float depththreshold = 0.03f;
        float normalthreshold = 0.95f;
        int contributioncap = 20;
        int candidatecap = 2;
        int spacerange = 5;
        int spacecount = 5;
    };

    class Renderer {
    public:
        Renderer() {};
        ~Renderer() {};
    public:
        void Initialize();
        void Render();
        void Resize(uint32_t width, uint32_t height);
        void LoadScene(std::string filepath);
    public:
        Backend& GetBackend() { return m_Backend; }
        CPUSwap& Swapchain() { return m_Swapchain; }
        Geometry& GetGeometry() { return m_Geometry; }
        camera& GetCamera() { return m_Camera; }

        RenderSettings& GetSettings() { return m_settings; }
    private:
        void RecordCommand(uint32_t imageIndex);
        bool ConstructOBJ(const StateOBJ state);
    private:
        CPUSwap m_Swapchain;
        Backend m_Backend;
        Geometry m_Geometry;
        camera m_Camera;
        RenderSettings m_settings;
    };

}
