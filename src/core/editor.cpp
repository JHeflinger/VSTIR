#include "editor.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

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
        glfwSetWindowUserPointer(s_Editor.m_Window, &s_Editor);
        glfwSetScrollCallback(s_Editor.m_Window, [](GLFWwindow* w, double, double yoff) {
            static_cast<Editor*>(glfwGetWindowUserPointer(w))->m_ScrollDelta += yoff;
        });
        s_Editor.m_Renderer.Initialize();
    }

    void Editor::LoadScene(std::string filepath) {
        s_Editor.m_Renderer.LoadScene(filepath);
    }

    void Editor::Run() {
        double prevMouseX = 0.0, prevMouseY = 0.0;
        glfwGetCursorPos(s_Editor.m_Window, &prevMouseX, &prevMouseY);

        while (!glfwWindowShouldClose(s_Editor.m_Window)) {
            glfwPollEvents();
            double mouseX, mouseY;
            glfwGetCursorPos(s_Editor.m_Window, &mouseX, &mouseY);
            double dx = mouseX - prevMouseX;
            double dy = mouseY - prevMouseY;
            prevMouseX = mouseX;
            prevMouseY = mouseY;
            Camera& cam = s_Editor.m_Renderer.GetCamera();
            bool changed = false;
            if (glfwGetMouseButton(s_Editor.m_Window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
                && (dx != 0.0 || dy != 0.0)) {
                const float sensitivity = 0.005f;
                glm::vec3 toCamera = cam.position - cam.look;
                float radius = glm::length(toCamera);
                glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), (float)-dx * sensitivity, glm::vec3(0, 1, 0));
                toCamera = glm::vec3(yaw * glm::vec4(toCamera, 0.0f));
                glm::vec3 right = glm::normalize(glm::cross(toCamera, glm::vec3(0, 1, 0)));
                glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), (float)dy * sensitivity, right);
                glm::vec3 pitched = glm::vec3(pitch * glm::vec4(toCamera, 0.0f));
                glm::vec3 pitchedNorm = glm::normalize(pitched);
                if (std::abs(pitchedNorm.y) < 0.99f)
                    toCamera = pitched;
                cam.position = cam.look + glm::normalize(toCamera) * radius;
                changed = true;
            }

            if (glfwGetMouseButton(s_Editor.m_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS
                && (dx != 0.0 || dy != 0.0)) {
                const float sensitivity = 0.002f;
                glm::vec3 forward = glm::normalize(cam.look - cam.position);
                glm::vec3 right   = glm::normalize(glm::cross(forward, cam.up));
                glm::vec3 up      = glm::normalize(glm::cross(right, forward));
                float dist        = glm::length(cam.look - cam.position);
                glm::vec3 pan = (-right * (float)dx + up * (float)dy) * sensitivity * dist;
                cam.position += pan;
                cam.look     += pan;
                changed = true;
            }

            double scroll = s_Editor.m_ScrollDelta;
            s_Editor.m_ScrollDelta = 0.0;
            if (scroll != 0.0) {
                const float sensitivity = 0.1f;
                glm::vec3 toCamera = cam.position - cam.look;
                float radius = glm::length(toCamera);
                radius = std::max(0.1f, radius - (float)scroll * sensitivity * radius);
                cam.position = cam.look + glm::normalize(toCamera) * radius;
                changed = true;
            }

            if (changed)
                s_Editor.m_Reset = true;

            s_Editor.m_Renderer.Render();
    }
}

    void Editor::Clean() {
        glfwDestroyWindow(s_Editor.m_Window);
        glfwTerminate();
    }

}
