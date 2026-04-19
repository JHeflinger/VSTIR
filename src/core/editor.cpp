#include "editor.h"
#include "core/ui.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

#include "get.h"
#include "keybinds.h"
#include "util/log.h"

namespace VSTIR {

    static Editor s_Editor;

    static bool IsInViewport(GLFWwindow* window, double xpos, double ypos) {

        (void)ypos;
        Editor* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        if (!editor || editor->Width() == 0 || editor->Height() == 0) {
            return false;
        }
        // Cursor coordinates are in logical window space, matching editor viewport getters.
        const double viewportWidth = (double)editor->ViewportWidth();
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
        s_Editor.m_pending_scene_path = std::move(filepath);
        s_Editor.m_has_pending_scene_load = true;
    }

    void Editor::Run() {
        double lastTime = glfwGetTime();

        while (!glfwWindowShouldClose(s_Editor.m_Window)) {
            glfwPollEvents();
            double currentTime = glfwGetTime();
            auto deltaTime = currentTime - lastTime;
            lastTime = currentTime;
            s_Editor.Update(deltaTime);
            s_Editor.m_Renderer.Render();
        }
    }

    void Editor::Clean() {
        glfwDestroyWindow(s_Editor.m_Window);
        glfwTerminate();
    }

    void Editor::KeyCallback(GLFWwindow* window, int key, int, int action, int) {
        Editor* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        // if (!editor || action != GLFW_PRESS) {
        //     return;
        // }
        if (action == GLFW_PRESS) {
            editor->m_inputs.keys_held[key] = true;
        }
        else if (action == GLFW_RELEASE) {
            editor->m_inputs.keys_held[key] = false;
        }
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    void Editor::MouseButtonCallback(GLFWwindow* window, int button, int action, int) {
        auto* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        if (!editor) {
            return;
        }
        double xpos = 0.0, ypos = 0.0;
        glfwGetCursorPos(window, &xpos, &ypos);
        const bool inViewport = IsInViewport(window, xpos, ypos);
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            editor->m_inputs.left_mouse_down = (action == GLFW_PRESS) && inViewport;
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            editor->m_inputs.right_mouse_down = (action == GLFW_PRESS) && inViewport;
        }
    }

    void Editor::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        Editor* editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
        if (!editor) {
            return;
        }

        if (!editor->m_inputs.mouse_initialized) {
            editor->m_inputs.prev_mouse_position = {xpos, ypos};
            editor->m_inputs.mouse_initialized = true;
            return;
        }


        const double dx = xpos - editor->m_inputs.prev_mouse_position.x;
        const double dy = ypos - editor->m_inputs.prev_mouse_position.y;
        editor->m_inputs.prev_mouse_position = {xpos, ypos};

        if (!IsInViewport(window, xpos, ypos)) {
            editor->m_inputs.left_mouse_down = false;
            editor->m_inputs.right_mouse_down = false;
            return;
        }

        if (dx == 0.0 && dy == 0.0) {
            return;
        }

        if (editor->m_inputs.left_mouse_down || editor->m_inputs.right_mouse_down) {
            _renderer.GetCamera().handleMouse(dx, dy);
            editor->m_camera_updated = true;
        }
        // if (editor->m_LeftMouseDown) {
        //     editor->HandlePan(dx, dy);
        //     editor->m_Reset = true;
        // }
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
        // Use logical window size (points) for UI layout — the framebuffer size passed here is
        // in physical pixels and would produce incorrect panel dimensions on Retina displays.
        int winW = 0, winH = 0;
        glfwGetWindowSize(window, &winW, &winH);


        if (winW > 0 && winH > 0) {
            editor->m_camera_updated = true;
            Get()->m_Width = winW;
            Get()->m_Height = winH;
            Get()->m_Renderer.Resize((uint32_t)width, (uint32_t)height);
            UI::onResize((float)winW, (float)winH);
        }
    }

    void Editor::Update(double delta_time) {
        if (m_has_pending_scene_load) {
            const std::string scenePath = m_pending_scene_path;
            m_pending_scene_path.clear();
            m_has_pending_scene_load = false;
            m_Renderer.LoadScene(scenePath);
            m_camera_updated = true;
        }

        UpdateCameraPosition(delta_time);

        // Reserved for per-frame editor updates that are not input callbacks.
    }

    void Editor::HandleOrbit(double dx, double dy) {

        // const float sensitivity = 0.005f;
        // glm::vec3 toCamera = cam.position - cam.look;
        // float radius = glm::length(toCamera);
        // glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), (float)-dx * sensitivity, glm::vec3(0, 1, 0));
        // toCamera = glm::vec3(yaw * glm::vec4(toCamera, 0.0f));
        // glm::vec3 right = glm::normalize(glm::cross(toCamera, glm::vec3(0, 1, 0)));
        // glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), (float)dy * sensitivity, right);
        // glm::vec3 pitched = glm::vec3(pitch * glm::vec4(toCamera, 0.0f));
        // glm::vec3 pitchedNorm = glm::normalize(pitched);
        // if (std::abs(pitchedNorm.y) < 0.99f) {
        //     toCamera = pitched;
        // }
        // cam.position = cam.look + glm::normalize(toCamera) * radius;

    }

    void Editor::HandlePan(double dx, double dy) {
        // Camera& cam = m_Renderer.GetCamera();
        // const float sensitivity = 0.002f;
        // glm::vec3 forward = glm::normalize(cam.look - cam.position);
        // glm::vec3 right   = glm::normalize(glm::cross(forward, cam.up));
        // glm::vec3 up      = glm::normalize(glm::cross(right, forward));
        // float dist        = glm::length(cam.look - cam.position);
        // glm::vec3 pan = (-right * (float)dx + up * (float)dy) * sensitivity * dist;
        // cam.position += pan;
        // cam.look += pan;
    }

    void Editor::HandleZoom(double yoffset) {
        auto& cam = m_Renderer.GetCamera();
        cam.handleZoom(yoffset);
        m_camera_updated = true;
    }

    void Editor::UpdateCameraPosition(double delta_time) {
        auto& cam = m_Renderer.GetCamera();

        if (!cam.IsOrbiting()) {
            glm::vec3 xz_direction = {0,0,0};
            glm::vec3 y_direction = {0,0,0};

            glm::vec3 dir = {0,0,0};
            if (m_inputs.keys_held[Keybinds::MoveForward]) {
                dir += cam.getLookXZ();
            }
            if (m_inputs.keys_held[Keybinds::MoveBackward]) {
                dir -= cam.getLookXZ();
            }
            if (m_inputs.keys_held[Keybinds::MoveRight]) { // right
                dir += glm::cross(cam.getLookXZ(), cam.getUp());
            }
            if (m_inputs.keys_held[Keybinds::MoveLeft]) {
                dir -= glm::cross(cam.getLookXZ(), cam.getUp());
            }
            if (m_inputs.keys_held[Keybinds::MoveUp]) {
                dir += cam.getUp();
            }
            if (m_inputs.keys_held[Keybinds::MoveDown]) {
                dir -= cam.getUp();
            }
            if (dir != glm::vec3(0,0,0)) {
                dir = glm::normalize(dir);
                cam.Position() += dir * cam.MovementSpeed() * float(delta_time);
                m_camera_updated = true;
            }

        }
    }
}
