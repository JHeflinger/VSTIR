#pragma once

#include <unordered_map>
#include <string>
#include <algorithm>

#include "util/safety.h"
#include "core/renderer.h"
#include <GLFW/glfw3.h>

namespace VSTIR {

    struct EditorVariables {

    };

    struct InputVariables {
        bool left_mouse_down = false;
        bool right_mouse_down = false;
        bool mouse_initialized = false;

        glm::vec2 prev_mouse_position;

        std::unordered_map<int, bool> keys_held;

    };

    class Editor {
    public:
        Editor() {};
        ~Editor() {};
        static constexpr float kViewportWidthRatio = 2.0f / 3.0f;
        static constexpr float kViewportHeightRatio = 1.0f;
    public:
        static Editor* Get();
        static void Initialize(size_t width, size_t height);
        static void LoadScene(std::string filepath);
        static void Run();
        static void Clean();
    private:
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
        void Update(double delta_time);
        void HandleOrbit(double dx, double dy);
        void HandlePan(double dx, double dy);
        void HandleZoom(double yoffset);
    public:

        void UpdateCameraPosition(double delta_time);
        Renderer& GetRenderer() { return m_Renderer; }
        GLFWwindow* Window() { return m_Window; }
        size_t Width() { return m_Width; }
        size_t Height() { return m_Height; }
        float ViewportWidthRatio() const { return kViewportWidthRatio; }
        float ViewportHeightRatio() const { return kViewportHeightRatio; }
        size_t ViewportWidth() const { return std::max<size_t>(1, (size_t)(m_Width * ViewportWidthRatio())); }
        size_t ViewportHeight() const { return std::max<size_t>(1, (size_t)(m_Height * ViewportHeightRatio())); }
        bool Reset() { bool r = m_camera_updated; m_camera_updated = false; return r; }
    // private:
        size_t m_Width = 0;
        size_t m_Height = 0;
        GLFWwindow* m_Window = nullptr;
        Renderer m_Renderer;
        InputVariables m_inputs;

        bool m_camera_updated = false;

        bool m_has_pending_scene_load = false;
        std::string m_pending_scene_path;
    };
}
