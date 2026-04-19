#pragma once
#include <glm/glm.hpp>


namespace VSTIR {
    class camera {
    public:
        camera() :
        m_position({0.0f, 2.133f, 2.11f}),
        m_look({0,0, 1}),
        m_up({0,1,0}),
        m_fov(90),
        m_orbiting(true),
        m_orbit_target({0,0,0})
        {}


        void setLook(glm::vec3 look);
        void setUp(glm::vec3 up);
        glm::vec3 getLook();
        glm::vec3 getLookXZ() { return glm::normalize(m_look * glm::vec3(1,0,1));}
        glm::vec3 getUp();
        void Reset();

        glm::vec3& Position() { return m_position; }
        float& Fov() { return m_fov; }
        bool& IsOrbiting() { return m_orbiting; }
        glm::vec3& OrbitTarget() { return m_orbit_target; }

        float& ZoomSensitivity() { return m_zoom_sensitivity; }
        float& LookSensitivity() { return m_look_sensitivity; }
        float& MovementSpeed() { return m_move_speed; }

        void handleMouse(double dx, double dy);
        void handleZoom(double yoffset);

    private:
        void handleOrbit(double dx, double dy);
        void handleFreeLook(double dx, double dy);

        glm::vec3 m_position;
        glm::vec3 m_look;
        glm::vec3 m_up;
        float m_fov;

        bool m_orbiting;
        glm::vec3 m_orbit_target;

        float m_move_speed = 1.f;
        float m_look_sensitivity = 1.f;
        float m_zoom_sensitivity = 1.f;
    };
    constexpr float LOOK_SENSITIVITY_FACTOR = 0.005f;
    constexpr float ZOOM_SENSITIVITY_FACTOR = 0.1f;
}