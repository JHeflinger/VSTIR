#include "editor.h"

namespace VSTIR {

    Editor::Editor(size_t width, size_t height) : m_Width(width), m_Height(height) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        m_Window = glfwCreateWindow(width, height, "VSTIR", nullptr, nullptr);
        m_Vulkan = CreateScope<VCore>(this);
    }

    Editor::~Editor() {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    void Editor::Run() {
        while (!glfwWindowShouldClose(m_Window)) {
            glfwPollEvents();
            m_Vulkan->Render();
        }
        m_Vulkan->Wait();
    }

}
