#ifndef LAYER_HPP
#define LAYER_HPP

#include "eng/event.hpp"
#include "eng/renderer/camera.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/scene/entity.hpp"
#include "eng/scene/scene.hpp"
#include "eng/window.hpp"
#include <memory>

struct Layer {
    virtual ~Layer() = default;

    virtual void on_attach() = 0;
    virtual void on_detach() = 0;

    virtual void on_event(eng::Event &event) = 0;
    virtual void on_update(float ts) = 0;
    virtual void on_tick(uint32_t tickrate) = 0;
    virtual void on_render() = 0;
};

struct EditorLayer : public Layer {
    virtual ~EditorLayer() = default;

    static std::unique_ptr<Layer> create(const eng::WindowSpec &win_spec);
    void destroy();

    virtual void on_attach() override;
    virtual void on_detach() override;

    virtual void on_event(eng::Event &event) override;
    virtual void on_update(float ts) override;
    virtual void on_tick(uint32_t tickrate) override;
    virtual void on_render() override;

    void render_control_panel();
    void render_entity_panel();

    eng::Scene scene;
    eng::AssetPack asset_pack;

    Framebuffer main_fbo;

    eng::SpectatorCamera camera;

    std::optional<eng::Entity> selected_entity;

    glm::vec2 viewport_pos;
};

#endif // LAYER_HPP
