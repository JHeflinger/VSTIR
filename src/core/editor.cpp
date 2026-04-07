#include "editor.h"

namespace VSTIR {

    static Editor s_Editor;

    Editor* Editor::Get() {
        return &s_Editor;
    }

    void Editor::Initialize(size_t width, size_t height) {
        s_Editor.m_Width = width;
        s_Editor.m_Height = height;
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        s_Editor.m_Window = glfwCreateWindow(width, height, "VSTIR", nullptr, nullptr);
        s_Editor.m_Renderer.Initialize();
    }

    void Editor::Run() {
        while (!glfwWindowShouldClose(s_Editor.m_Window)) {
            glfwPollEvents();
            //m_Vulkan->Render();
            break;
        }
        //m_Vulkan->Wait();
    }

    void Editor::Clean() {
        glfwDestroyWindow(s_Editor.m_Window);
        glfwTerminate();
    }

}
