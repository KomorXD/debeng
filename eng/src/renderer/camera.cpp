#include "eng/renderer/camera.hpp"
#include "eng/input.hpp"

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
    return glm::quat(glm::vec3(-glm::radians(pitch), -glm::radians(yaw),
                               -glm::radians(roll)));
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

renderer::CameraData SpectatorCamera::camera_render_data() const {
    renderer::CameraData cdata;
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

void SpectatorCamera::update_with_input(float timestep) {
    switch (control_mode) {
    case ControlMode::FPS:
        fps_update(timestep);
        break;

    case ControlMode::TRACKBALL:
        trackball_update(timestep);
        break;

    default:
        break;
    }
}

void SpectatorCamera::fps_update(float timestep) {
    glm::vec3 move(0.0f);

    if (is_key_pressed(Key::W))
        move += forward_dir();
    else if (is_key_pressed(Key::S))
        move -= forward_dir();

    if (is_key_pressed(Key::A))
        move -= right_dir();
    else if (is_key_pressed(Key::D))
        move += right_dir();

    if (is_key_pressed(Key::Space))
        move.y += 1.0f;
    else if (is_key_pressed(Key::LeftShift))
        move.y -= 1.0f;

    if (glm::length2(move) != 0.0f)
        position += glm::normalize(move) * moving_speed_ps * timestep;

    glm::vec2 mouse_delta = get_mouse_move_delta();
    yaw += mouse_delta.x * mouse_sens;
    pitch -= mouse_delta.y * mouse_sens;
    pitch = glm::clamp(pitch, -90.0f, 90.0f);

    if (is_key_pressed(Key::Left))
        roll -= rolling_angle_ps * timestep;
    else if (is_key_pressed(Key::Right))
        roll += rolling_angle_ps * timestep;
}

void SpectatorCamera::trackball_update(float timestep) {
    glm::vec2 mouse_delta = get_mouse_move_delta();

    if (is_mouse_btn_pressed(MouseButton::Right)) {
        disable_cursor();

        yaw += mouse_delta.x * mouse_sens;
        pitch -= mouse_delta.y * mouse_sens;
        pitch = glm::clamp(pitch, -90.0f, 90.0f);

        return;
    }

    if (is_mouse_btn_pressed(MouseButton::Middle)) {
        disable_cursor();

        position -= right_dir() * mouse_delta.x * 0.02f;
        position -= up_dir() * mouse_delta.y * 0.02f;

        return;
    }

    enable_cursor();
}

} // namespace eng
