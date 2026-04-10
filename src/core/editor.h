#pragma once

#include "util/safety.h"
#include "core/renderer.h"
#include <GLFW/glfw3.h>

namespace VSTIR {

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
    public:
        Renderer& GetRenderer() { return m_Renderer; }
        GLFWwindow* Window() { return m_Window; }
        size_t Width() { return m_Width; }
        size_t Height() { return m_Height; }
    private:
        size_t m_Width = 0;
        size_t m_Height = 0;
        GLFWwindow* m_Window = nullptr;
        Renderer m_Renderer;
    };
}
