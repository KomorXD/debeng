#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "eng/renderer/renderer.hpp"

namespace eng {

struct SpectatorCamera {
    glm::vec3 up_dir() const;
    glm::vec3 right_dir() const;
    glm::vec3 forward_dir() const;
    glm::quat orientation() const;

    glm::mat4 projection() const;
    glm::mat4 view() const;

    Renderer::CameraData camera_render_data() const;

    glm::vec3 position;
    glm::vec2 viewport;
    float fov = 90.0f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;

    float pitch = 0.0f;
    float yaw = 0.0f;

    float exposure = 1.0f;
    float gamma = 2.2f;
};

} // namespace eng

#endif
