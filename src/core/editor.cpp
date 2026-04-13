#include "editor.h"
#include "core/ui.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace VSTIR {

    static Editor s_Editor;

    static bool IsInViewport(GLFWwindow* window, double xpos, double ypos) {
        (void)ypos;
        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        if (fbWidth <= 0 || fbHeight <= 0) {
            return false;
        }
        const double viewportWidth = (double)fbWidth * (2.0 / 3.0);
        return xpos >= 0.0 && xpos < viewportWidth;
    }

    Editor* Editor::Get() {
        return &s_Editor;
    }

    void Editor::Initialize(size_t width, size_t height) {
        s_Editor.m_Width = width;
        s_Editor.m_Height = height;
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
#ifdef __APPLE__ // Disable retina scaling
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif
        s_Editor.m_Window = glfwCreateWindow(width, height, "VSTIR", nullptr, nullptr);
        glfwSetWindowUserPointer(s_Editor.m_Window, &s_Editor);
        glfwSetKeyCallback(s_Editor.m_Window, KeyCallback);
        glfwSetMouseButtonCallback(s_Editor.m_Window, MouseButtonCallback);
        glfwSetCursorPosCallback(s_Editor.m_Window, CursorPosCallback);
        glfwSetScrollCallback(s_Editor.m_Window, ScrollCallback);
        s_Editor.m_Renderer.Initialize();
        glfwSetFramebufferSizeCallback(s_Editor.m_Window, FramebufferResizeCallback);
        UI::onResize((float)width, (float)height);
    }

    void Editor::LoadScene(std::string filepath) {
        s_Editor.m_Renderer.LoadScene(filepath);
    }

    void Editor::Run() {
        while (!glfwWindowShouldClose(s_Editor.m_Window)) {
            glfwPollEvents();
            s_Editor.Update();
            s_Editor.m_Renderer.Render();
        }
    }

    void Editor::Clean() {
        glfwDestroyWindow(s_Editor.m_Window);
        glfwTerminate();
    }

    void Editor::KeyCallback(GLFWwindow* window, int key, int, int action, int) {
        Editor* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        if (!editor || action != GLFW_PRESS) {
            return;
        }
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    void Editor::MouseButtonCallback(GLFWwindow* window, int button, int action, int) {
        Editor* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        if (!editor) {
            return;
        }
        double xpos = 0.0, ypos = 0.0;
        glfwGetCursorPos(window, &xpos, &ypos);
        const bool inViewport = IsInViewport(window, xpos, ypos);
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            editor->m_LeftMouseDown = (action == GLFW_PRESS) && inViewport;
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            editor->m_RightMouseDown = (action == GLFW_PRESS) && inViewport;
        }
    }

    void Editor::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        Editor* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        if (!editor) {
            return;
        }

        if (!editor->m_MouseInitialized) {
            editor->m_PrevMouseX = xpos;
            editor->m_PrevMouseY = ypos;
            editor->m_MouseInitialized = true;
            return;
        }

        const double dx = xpos - editor->m_PrevMouseX;
        const double dy = ypos - editor->m_PrevMouseY;
        editor->m_PrevMouseX = xpos;
        editor->m_PrevMouseY = ypos;

        if (!IsInViewport(window, xpos, ypos)) {
            editor->m_LeftMouseDown = false;
            editor->m_RightMouseDown = false;
            return;
        }

        if (dx == 0.0 && dy == 0.0) {
            return;
        }

        if (editor->m_RightMouseDown) {
            editor->HandleOrbit(dx, dy);
            editor->m_Reset = true;
        }
        if (editor->m_LeftMouseDown) {
            editor->HandlePan(dx, dy);
            editor->m_Reset = true;
        }
    }

    void Editor::ScrollCallback(GLFWwindow* window, double, double yoffset) {
        Editor* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        if (!editor) {
            return;
        }
        double xpos = 0.0, ypos = 0.0;
        glfwGetCursorPos(window, &xpos, &ypos);
        if (!IsInViewport(window, xpos, ypos)) {
            return;
        }
        editor->HandleZoom(yoffset);
    }

    void Editor::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
        Editor* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        if (!editor || width <= 0 || height <= 0) {
            return;
        }
        // Let the render loop own swapchain recreation; callbacks can fire rapidly during drags.
        UI::onResize((float)width, (float)height);
    }

    void Editor::Update() {
        // Reserved for per-frame editor updates that are not input callbacks.
    }

    void Editor::HandleOrbit(double dx, double dy) {
        Camera& cam = m_Renderer.GetCamera();
        const float sensitivity = 0.005f;
        glm::vec3 toCamera = cam.position - cam.look;
        float radius = glm::length(toCamera);
        glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), (float)-dx * sensitivity, glm::vec3(0, 1, 0));
        toCamera = glm::vec3(yaw * glm::vec4(toCamera, 0.0f));
        glm::vec3 right = glm::normalize(glm::cross(toCamera, glm::vec3(0, 1, 0)));
        glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), (float)dy * sensitivity, right);
        glm::vec3 pitched = glm::vec3(pitch * glm::vec4(toCamera, 0.0f));
        glm::vec3 pitchedNorm = glm::normalize(pitched);
        if (std::abs(pitchedNorm.y) < 0.99f) {
            toCamera = pitched;
        }
        cam.position = cam.look + glm::normalize(toCamera) * radius;
    }

    void Editor::HandlePan(double dx, double dy) {
        Camera& cam = m_Renderer.GetCamera();
        const float sensitivity = 0.002f;
        glm::vec3 forward = glm::normalize(cam.look - cam.position);
        glm::vec3 right   = glm::normalize(glm::cross(forward, cam.up));
        glm::vec3 up      = glm::normalize(glm::cross(right, forward));
        float dist        = glm::length(cam.look - cam.position);
        glm::vec3 pan = (-right * (float)dx + up * (float)dy) * sensitivity * dist;
        cam.position += pan;
        cam.look += pan;
    }

    void Editor::HandleZoom(double yoffset) {
        if (yoffset == 0.0) {
            return;
        }
        Camera& cam = m_Renderer.GetCamera();
        const float sensitivity = 0.1f;
        glm::vec3 toCamera = cam.position - cam.look;
        float radius = glm::length(toCamera);
        radius = std::max(0.1f, radius - (float)yoffset * sensitivity * radius);
        cam.position = cam.look + glm::normalize(toCamera) * radius;
        m_Reset = true;
    }

}
