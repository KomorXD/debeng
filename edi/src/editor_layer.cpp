#include "eng/containers/registry.hpp"
#include "eng/event.hpp"
#include "eng/input.hpp"
#include "eng/renderer/camera.hpp"
#include "eng/renderer/renderer.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include "glm/fwd.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "layers.hpp"

std::unique_ptr<Layer> EditorLayer::create(const eng::WindowSpec &win_spec) {
    glm::ivec2 window_size = glm::ivec2(win_spec.width, win_spec.height);

    std::unique_ptr<EditorLayer> layer = std::make_unique<EditorLayer>();
    layer->camera.position = glm::vec3(0.0f, 2.0f, -3.0f);
    layer->camera.viewport = window_size;
    layer->camera.control_mode = eng::SpectatorCamera::ControlMode::FPS;
    eng::disable_cursor();

    layer->main_fbo = Framebuffer::create();
    layer->main_fbo.add_renderbuffer({RenderbufferType::DEPTH, window_size});
    layer->main_fbo.add_color_attachment({.type = ColorAttachmentType::TEX_2D,
                                          .format = TextureFormat::RGBA8,
                                          .wrap = GL_CLAMP_TO_EDGE,
                                          .min_filter = GL_LINEAR,
                                          .mag_filter = GL_LINEAR,
                                          .size = window_size,
                                          .gen_minmaps = false});

    layer->asset_pack = eng::AssetPack::create("default");

    layer->scene = eng::Scene::create("New scene");

    eng::Entity ent = layer->scene.spawn_entity("ent1");
    ent.add_component<eng::MeshComp>().id = eng::AssetPack::CUBE_ID;
    ent.add_component<eng::MaterialComp>().id =
        eng::AssetPack::DEFAULT_MATERIAL;

    layer->selected_entity = ent;

    ent = layer->scene.spawn_entity("light");
    ent.get_component<eng::Transform>().position = glm::vec3(2.0f, 2.0f, 1.0f);
    ent.add_component<eng::PointLight>().intensity = 3.0f;

    return layer;
}

void EditorLayer::destroy() {
    scene.destroy();
    asset_pack.destroy();
    main_fbo.destroy();
}

void EditorLayer::on_attach() {}

void EditorLayer::on_detach() {}

void EditorLayer::on_event(eng::Event &event) {
    switch (event.type) {
    case eng::EventType::KeyPressed:
        if (event.key.key == eng::Key::Escape)
            eng::Window::terminate();
        else if (event.key.key == eng::Key::Q) {
            camera.control_mode = eng::SpectatorCamera::ControlMode::FPS;
            eng::disable_cursor();
        } else if (event.key.key == eng::Key::E) {
            camera.control_mode = eng::SpectatorCamera::ControlMode::TRACKBALL;
            eng::enable_cursor();
        }

        break;
    default:
        break;
    }
}

void EditorLayer::on_update(float ts) {
    camera.update_with_input(ts);
}

void EditorLayer::on_tick(uint32_t tickrate) {}

void EditorLayer::on_render() {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("main_dockspace", nullptr,
                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("dockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);

    static bool first_time = true;
    if (first_time) {
        first_time = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id,
                                  ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        ImGuiID left = ImGui::DockBuilderSplitNode(
            dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
        ImGuiID right = ImGui::DockBuilderSplitNode(
            dockspace_id, ImGuiDir_Right, 0.2f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow("dock_left", left);
        ImGui::DockBuilderDockWindow("dock_main", dockspace_id);
        ImGui::DockBuilderDockWindow("Entity panel", right);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();

    ImGui::Begin("dock_left");
    ImGui::Text("left");
    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("dock_main", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImVec2 avail_region = ImGui::GetContentRegionAvail();
    ImGui::Image((ImTextureID)main_fbo.color_attachments[0].id, avail_region,
                 {0.0f, 1.0f}, {1.0f, 0.0f});
    ImGui::PopStyleVar();
    ImGui::End();

    ImGui::Begin("Entity panel");
    render_entity_panel();
    ImGui::End();

    glm::ivec2 avail_region_iv2 = {(int32_t)avail_region.x,
                                   (int32_t)avail_region.y};
    camera.viewport = avail_region_iv2;

    main_fbo.bind();
    main_fbo.resize_everything(avail_region_iv2);
    main_fbo.bind_renderbuffer();
    main_fbo.bind_color_attachment(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.33f, 0.33f, 0.33f, 1.0f);

    eng::renderer::scene_begin(camera.camera_render_data(), asset_pack);

    eng::ecs::RegistryView rview =
        scene.registry.view<eng::Transform, eng::PointLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::PointLight &light = rview.get<eng::PointLight>(entry);

        eng::renderer::submit_point_light(transform.position, light);
    }

    rview =
        scene.registry.view<eng::Transform, eng::MeshComp, eng::MaterialComp>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::MeshComp &mesh = rview.get<eng::MeshComp>(entry);
        eng::MaterialComp &mat = rview.get<eng::MaterialComp>(entry);

        eng::renderer::submit_mesh(transform.to_mat4(), mesh.id, mat.id);
    }

    eng::renderer::scene_end();

    main_fbo.unbind();
}

void EditorLayer::render_entity_panel() {
    if (!selected_entity.has_value())
        return;

    eng::Entity ent = selected_entity.value();

    eng::Name &name = ent.get_component<eng::Name>();
    char buf[128] = { 0 };
    strncpy(buf, name.name.data(), 128);

    float horizontal_size = ImGui::CalcTextSize("Name").x;
    ImGui::Indent(16.0f);
    ImGui::PrettyInputText("Name", buf, horizontal_size);
    ImGui::Unindent(16.0f);

    name.name = buf;

    eng::Transform &transform = ent.get_component<eng::Transform>();
    ImGui::PushID(1);
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        horizontal_size = ImGui::CalcTextSize("Position").x;
        ImGui::Indent(16.0f);
        ImGui::PrettyDragFloat3("Position", &transform.position[0], 0.05f, 0.0f,
                                0.0f, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat3("Rotation", &transform.rotation[0], 0.05f, 0.0f,
                                0.0f, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat3("Scale", &transform.scale[0], 0.05f, 0.0f, 0.0f,
                                "%.3f", horizontal_size);
        ImGui::Unindent(16.0f);
    }
    ImGui::PopID();

    ImGui::PushID(2);
    if (ent.has_component<eng::MeshComp>()) {
        eng::MeshComp &mesh_comp = ent.get_component<eng::MeshComp>();

        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            ImGui::BeginPrettyCombo(
                "Mesh", asset_pack.meshes.at(mesh_comp.id).name.c_str(),
                [this, &mesh_comp]() {
                    const std::map<eng::AssetID, eng::Mesh> &meshes =
                        asset_pack.meshes;

                    for (const auto &[id, meshData] : meshes) {
                        if (ImGui::Selectable(meshData.name.c_str(),
                                              id == mesh_comp.id)) {
                            mesh_comp.id = id;
                        }
                    }
                });

            if (ImGui::PrettyButton("Remove component"))
                ent.remove_component<eng::MeshComp>();

            ImGui::Unindent(8.0f);
        }
    }
    ImGui::PopID();
}
