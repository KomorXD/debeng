#include "eng/renderer/camera.hpp"
#include "eng/input.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace eng {

std::unique_ptr<CameraControl> TrackballControl::create(SpectatorCamera *cam) {
    assert(cam != nullptr && "Invalid camera object");

    std::unique_ptr<TrackballControl> ret =
        std::make_unique<TrackballControl>();
    ret->camera = cam;

    return ret;
}

void TrackballControl::on_event(Event &ev) {
    switch (ev.type) {
    case eng::EventType::MouseWheelScrolled:
        camera->position += ev.mouse_scroll.offset_y * camera->forward_dir();
        return;
    default:
        break;
    }
}

void TrackballControl::on_update(float timestep) {
    glm::vec2 mouse_delta = get_mouse_move_delta();

    if (is_mouse_btn_pressed(MouseButton::Right)) {
        disable_cursor();

        camera->yaw += mouse_delta.x * camera->mouse_sens;
        camera->pitch -= mouse_delta.y * camera->mouse_sens;
        camera->pitch = glm::clamp(camera->pitch, -90.0f, 90.0f);

        return;
    }

    if (is_mouse_btn_pressed(MouseButton::Middle)) {
        disable_cursor();

        camera->position -= camera->right_dir() * mouse_delta.x * 0.02f;
        camera->position -= camera->up_dir() * mouse_delta.y * 0.02f;

        return;
    }

    enable_cursor();
}

std::unique_ptr<CameraControl> OrbitalControl::create(SpectatorCamera *cam,
                                                      glm::vec3 *target) {
    assert(cam != nullptr && "Invalid camera object");
    assert(target != nullptr && "Invalid target position");

    std::unique_ptr<OrbitalControl> ret = std::make_unique<OrbitalControl>();
    ret->camera = cam;
    ret->target_pos = target;
    ret->distance = glm::distance(cam->position, *target);

    return ret;
}

void OrbitalControl::on_event(Event &ev) {
    switch (ev.type) {
    case eng::EventType::MouseWheelScrolled:
        camera->position += ev.mouse_scroll.offset_y * camera->forward_dir();
        return;
    default:
        break;
    }
}

void OrbitalControl::on_update(float timestep) {
    glm::vec2 mouse_delta = get_mouse_move_delta();

    if (is_mouse_btn_pressed(MouseButton::Right)) {
        camera->yaw += mouse_delta.x * camera->mouse_sens;
        camera->pitch -= mouse_delta.y * camera->mouse_sens;
        camera->pitch = glm::clamp(camera->pitch, -90.0f, 90.0f);

        disable_cursor();
    } else
        enable_cursor();

    glm::vec3 forward = camera->forward_dir();
    distance = glm::distance(camera->position, *target_pos);
    camera->position = *target_pos - forward * distance;
}

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

renderer::CameraData SpectatorCamera::render_data() const {
    renderer::CameraData cdata;
    cdata.projection = projection();
    cdata.view = view();
    cdata.view_projection = cdata.projection * cdata.view;
    cdata.position = glm::vec4(position, 1.0f);
    cdata.viewport = viewport;

    cdata.gamma = gamma;
    cdata.exposure = exposure;
    cdata.near_clip = near_clip;
    cdata.far_clip = far_clip;
    cdata.fov = fov;

    cdata.bloom_strength = bloom_strength;
    cdata.bloom_threshold = bloom_threshold;
    cdata.bloom_mip_radius = bloom_mip_radius;

    return cdata;
}

void SpectatorCamera::on_event(Event &ev) {
    cam_control->on_event(ev);
}

void SpectatorCamera::on_update(float timestep) {
    cam_control->on_update(timestep);
}

} // namespace eng
