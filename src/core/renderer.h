#pragma once

#include "vulkan/backend.h"
#include "vulkan/vstructs.h"
#include <cstddef>

#include "camera.h"

namespace VSTIR {

    class Editor;

    struct RenderSettings {
        bool accumulate_samples = false;
        int32_t sample_count = 0;
        uint32_t restir_history_count = 0;

        float resolution_scale = 1.0f;
        uint32_t _last_render_width  = 0; // internal: used to detect render-size changes
        uint32_t _last_render_height = 0;

        bool denoiser = false; // TODO implement
        bool show_divider = false;
        float divider_position = 0.5f;
        float divider_angle = 0.0f;
        bool restir = false;
        bool restir_right = false;

        float depththreshold = 0.03f;
        float normalthreshold = 0.95f;
        int temporal_m_cap = 30;
        int spatial_m_cap = 500;
        int spacerange = 5;
        int spacecount = 5;
        int restir_bounces = 2;
        bool bilateral = false;
        bool bilateral_right = false;
        bool directlighting = true;
        bool directlighting_right = true;
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
    private:
        CPUSwap m_Swapchain;
        Backend m_Backend;
        Geometry m_Geometry;
        camera m_Camera;
        RenderSettings m_settings;
    };

}
