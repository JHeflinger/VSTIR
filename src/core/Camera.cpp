#include "Camera.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace VSTIR {
    void Camera::setLook(const glm::vec3 look) {
        m_look = glm::normalize(look);
    }

    void Camera::setUp(const glm::vec3 up) {
        m_up = glm::normalize(up);
    }

    glm::vec3 Camera::getLook() {
        if (m_orbiting) {
            m_look = glm::normalize(m_orbit_target - m_position);
        }
        return m_look;
    }

    glm::vec3 Camera::getUp() {
        return m_up;
    }

    void Camera::Reset() {
        m_position = {0.0f, 2.133f, 2.11f};
        m_look = {0,0, 1};
        m_up = {0,1,0};
        m_fov = 90;
        m_orbiting = true;
        m_orbit_target = {0,0,0};
    }


    void Camera::handleMouse(double dx, double dy) {
        if (m_orbiting) {
            handleOrbit(dx, dy);
        } else {
            handleFreeLook(dx, dy);
        }
    }

    void Camera::handleOrbit(double dx, double dy) {
        glm::vec3 toCamera = m_position - m_orbit_target;
        float radius = glm::length(toCamera);
        glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), (float)-dx * m_look_sensitivity * LOOK_SENSITIVITY_FACTOR, glm::vec3(0, 1, 0));
        toCamera = glm::vec3(yaw * glm::vec4(toCamera, 0.0f));
        glm::vec3 right = glm::normalize(glm::cross(toCamera, glm::vec3(0, 1, 0)));
        glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), (float)dy * m_look_sensitivity * LOOK_SENSITIVITY_FACTOR, right);
        glm::vec3 pitched = glm::vec3(pitch * glm::vec4(toCamera, 0.0f));
        glm::vec3 pitchedNorm = glm::normalize(pitched);
        if (std::abs(pitchedNorm.y) < 0.99f) {
            toCamera = pitched;
        }
        m_position = m_orbit_target + glm::normalize(toCamera) * radius;
        m_look = -glm::normalize(toCamera);
    }

    void Camera::handleZoom(double yoffset) {
        if (!m_orbiting) return;
        if (yoffset == 0.0) return;

        glm::vec3 toCamera = m_position - m_orbit_target;
        float radius = glm::length(toCamera);
        radius = std::max(0.1f, radius - (float)yoffset * m_zoom_sensitivity * ZOOM_SENSITIVITY_FACTOR * radius);
        m_position = m_orbit_target + glm::normalize(toCamera) * radius;
    }

    void Camera::handleFreeLook(double dx, double dy) {
        // Yaw: rotate m_look around the world up axis
        glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), (float)-dx * m_look_sensitivity * LOOK_SENSITIVITY_FACTOR, glm::vec3(0, 1, 0));
        m_look = glm::normalize(glm::vec3(yaw * glm::vec4(m_look, 0.0f)));

        // Pitch: rotate m_look around the camera's local right axis
        glm::vec3 right = glm::normalize(glm::cross(m_look, glm::vec3(0, 1, 0)));
        glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), (float)-dy * m_look_sensitivity * LOOK_SENSITIVITY_FACTOR, right);
        glm::vec3 pitched = glm::normalize(glm::vec3(pitch * glm::vec4(m_look, 0.0f)));

        // Clamp to avoid gimbal flip at the poles
        if (std::abs(pitched.y) < 0.99f) {
            m_look = pitched;
        }
    }
}
