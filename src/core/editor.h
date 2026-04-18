#pragma once

#include <unordered_map>

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
        bool Reset() { bool r = m_Reset; m_Reset = false; return r; }
    // private:
        size_t m_Width = 0;
        size_t m_Height = 0;
        GLFWwindow* m_Window = nullptr;
        Renderer m_Renderer;
        InputVariables m_inputs;

        bool m_Reset = false;
    };
}
