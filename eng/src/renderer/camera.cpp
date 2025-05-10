#include "eng/renderer/camera.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace eng {

glm::vec3 SpectatorCamera::up_dir() const {
    return glm::rotate(orientation(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 SpectatorCamera::right_dir() const {
    return glm::rotate(orientation(), glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 SpectatorCamera::forward_dir() const {
    return glm::rotate(orientation(), glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::quat SpectatorCamera::orientation() const {
    return glm::quat(glm::vec3(-glm::radians(pitch), -glm::radians(yaw), 0.0f));
}

glm::mat4 SpectatorCamera::projection() const {
    float aspect_ratio = (float)viewport.x / viewport.y;
    return glm::perspective(glm::radians(fov), aspect_ratio, near_clip,
                            far_clip);
}

glm::mat4 SpectatorCamera::view() const {
    glm::mat4 view_mat =
        glm::translate(glm::mat4(1.0f), position) * glm::toMat4(orientation());

    return glm::inverse(view_mat);
}

Renderer::CameraData SpectatorCamera::camera_render_data() const {
    Renderer::CameraData cdata;
    cdata.view = view();
    cdata.far_clip = far_clip;
    cdata.near_clip = near_clip;
    cdata.projection = projection();
    cdata.position = position;
    cdata.gamma = gamma;
    cdata.exposure = exposure;
    cdata.viewport = viewport;

    return cdata;
}

} // namespace eng
