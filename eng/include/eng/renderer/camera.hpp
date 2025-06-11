#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "eng/renderer/renderer.hpp"

namespace eng {

struct SpectatorCamera {
    enum class ControlMode {
        NONE,
        FPS,
        TRACKBALL
    };

    [[nodiscard]] glm::vec3 up_dir() const;
    [[nodiscard]] glm::vec3 right_dir() const;
    [[nodiscard]] glm::vec3 forward_dir() const;
    [[nodiscard]] glm::quat orientation() const;

    [[nodiscard]] glm::mat4 projection() const;
    [[nodiscard]] glm::mat4 view() const;

    [[nodiscard]] renderer::CameraData camera_render_data() const;

    void update_with_input(float timestep);
    void fps_update(float timestep);
    void trackball_update(float timestep);

    glm::vec3 position;
    glm::vec2 viewport;
    float fov = 90.0f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;

    float pitch = 0.0f;
    float yaw = 0.0f;
    float roll = 0.0f;

    float exposure = 1.0f;
    float gamma = 2.2f;

    ControlMode control_mode;
    float moving_speed_ps = 10.0f;
    float rolling_angle_ps = 180.0f;
    float mouse_sens = 0.1f;
};

} // namespace eng

#endif
