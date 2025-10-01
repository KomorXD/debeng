#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "eng/event.hpp"
#include "eng/renderer/renderer.hpp"
#include <memory>

namespace eng {

struct SpectatorCamera;

struct CameraControl {
    virtual ~CameraControl() = default;

    virtual void on_event(Event &ev) = 0;
    virtual void on_update(float timestep) = 0;
};

struct TrackballControl : public CameraControl {
    static std::unique_ptr<CameraControl> create(SpectatorCamera *cam);

    void on_event(Event &ev) override;
    void on_update(float timestep) override;

    SpectatorCamera *camera = nullptr;
};

struct OrbitalControl : public CameraControl {
    static std::unique_ptr<CameraControl> create(SpectatorCamera *cam,
                                                 glm::vec3 *target);

    void on_event(Event &ev) override;
    void on_update(float timestep) override;

    SpectatorCamera *camera = nullptr;
    glm::vec3 *target_pos = nullptr;
    float distance = 0.0f;
};

struct SpectatorCamera {
    [[nodiscard]] glm::vec3 up_dir() const;
    [[nodiscard]] glm::vec3 right_dir() const;
    [[nodiscard]] glm::vec3 forward_dir() const;
    [[nodiscard]] glm::quat orientation() const;

    [[nodiscard]] glm::mat4 projection() const;
    [[nodiscard]] glm::mat4 view() const;

    [[nodiscard]] renderer::CameraData render_data() const;

    void on_event(Event &ev);
    void on_update(float timestep);

    std::unique_ptr<CameraControl> cam_control;

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

    float bloom_strength = 1.0f;
    float bloom_threshold = 1.0f;

    float moving_speed_ps = 10.0f;
    float rolling_angle_ps = 180.0f;
    float mouse_sens = 0.1f;
};

} // namespace eng

#endif
