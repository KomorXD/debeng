#ifndef LAYER_HPP
#define LAYER_HPP

#include "eng/event.hpp"
#include "eng/renderer/camera.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/entity.hpp"
#include "eng/scene/scene.hpp"
#include "eng/window.hpp"
#include "imgui/ImGuizmo.h"
#include <memory>

struct Layer {
    virtual ~Layer() = default;

    virtual void destroy() = 0;

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
    virtual void destroy() override;

    virtual void on_attach() override;
    virtual void on_detach() override;

    virtual void on_event(eng::Event &event) override;
    virtual void on_update(float ts) override;
    virtual void on_tick(uint32_t tickrate) override;
    virtual void on_render() override;

    eng::Scene scene;
    eng::AssetPack asset_pack;

    eng::AssetID envmap_id;

    Framebuffer main_fbo;

    eng::SpectatorCamera camera;

    std::optional<eng::Entity> selected_entity;
    eng::AssetID outline_material;

    ImGuizmo::OPERATION gizmo_op = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE gizmo_mode = ImGuizmo::WORLD;

    glm::vec2 viewport_pos;

    bool viewport_hovered = false;
    bool lock_focus = false;
};

#endif // LAYER_HPP
