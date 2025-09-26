#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "context.hpp"
#include "eng/containers/registry.hpp"
#include "eng/event.hpp"
#include "eng/input.hpp"
#include "eng/random_utils.hpp"
#include "eng/renderer/camera.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/renderer/renderer.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include "eng/scene/entity.hpp"
#include "eng/scene/scene.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/trigonometric.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuizmo.h"
#include "layers.hpp"
#include <cfloat>
#include <string>
#include <signal.h>

std::unique_ptr<Layer> EditorLayer::create(const eng::WindowSpec &win_spec) {
    glm::ivec2 window_size = glm::ivec2(win_spec.width, win_spec.height);

    std::unique_ptr<EditorLayer> layer = std::make_unique<EditorLayer>();
    layer->camera.position = glm::vec3(0.0f, 2.0f, -3.0f);
    layer->camera.yaw = 180.0f;
    layer->camera.viewport = window_size;
    layer->camera.cam_control = eng::TrackballControl::create(&layer->camera);
    eng::disable_cursor();

    layer->main_fbo = Framebuffer::create();
    layer->main_fbo.add_renderbuffer(
        {RenderbufferType::DEPTH_STENCIL, window_size});
    layer->main_fbo.add_color_attachment({.type = ColorAttachmentType::TEX_2D,
                                          .format = TextureFormat::RGB16F,
                                          .wrap = GL_CLAMP_TO_EDGE,
                                          .min_filter = GL_NEAREST,
                                          .mag_filter = GL_NEAREST,
                                          .size = window_size,
                                          .gen_minmaps = false});
    layer->main_fbo.add_color_attachment({.type = ColorAttachmentType::TEX_2D,
                                          .format = TextureFormat::RGBA8,
                                          .wrap = GL_CLAMP_TO_EDGE,
                                          .min_filter = GL_NEAREST,
                                          .mag_filter = GL_NEAREST,
                                          .size = window_size,
                                          .gen_minmaps = false});
    layer->main_fbo.add_color_attachment({.type = ColorAttachmentType::TEX_2D,
                                          .format = TextureFormat::RGB16F,
                                          .wrap = GL_CLAMP_TO_EDGE,
                                          .min_filter = GL_NEAREST,
                                          .mag_filter = GL_NEAREST,
                                          .size = window_size,
                                          .gen_minmaps = false});

    layer->asset_pack = eng::AssetPack::create("default");

    layer->scene = eng::Scene::create("New scene");

    eng::Entity ent = layer->scene.spawn_entity("ent");
    ent.get_component<eng::Transform>().scale = glm::vec3(10.0f, 1.0f, 10.0f);
    ent.add_component<eng::MeshComp>().id = eng::AssetPack::CUBE_ID;
    ent.add_component<eng::MaterialComp>().id =
        eng::AssetPack::DEFAULT_BASE_MATERIAL;

    ent = layer->scene.spawn_entity("light");
    ent.get_component<eng::Transform>().position = glm::vec3(0.0f, 4.0f, 0.0f);
    ent.add_component<eng::PointLight>().intensity = 3.0f;
    ent.add_component<eng::MeshComp>().id = eng::AssetPack::CUBE_ID;
    ent.add_component<eng::MaterialComp>().id =
        eng::AssetPack::DEFAULT_FLAT_MATERIAL;

    eng::Material mat;
    mat.name = "Outline";
    mat.color = glm::vec4(0.76f, 0.20f, 0.0f, 1.0f);
    mat.shader_id = eng::AssetPack::DEFAULT_FLAT_MATERIAL;
    layer->outline_material = layer->asset_pack.add_material(mat);

    return layer;
}

void EditorLayer::destroy() {
    scene.destroy();
    asset_pack.destroy();
    main_fbo.destroy();
}

static void sig_handler(int signo) {
    if (signo == SIGTERM)
        context()->close_app();
}

void EditorLayer::on_attach() {
    struct sigaction act;
    act.sa_handler = sig_handler;
    sigfillset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    if (sigaction (SIGTERM, &act, NULL) == -1)
        fprintf(stderr,
                "Could not install handler for SIGTERM, closing an app might "
                "be not working correctly. If so, shutdown forcibly.\n");
}

void EditorLayer::on_detach() {}

void EditorLayer::on_event(eng::Event &event) {
    switch (event.type) {
    case eng::EventType::KeyPressed:
        switch (event.key.key) {
        case eng::Key::Escape:
            selected_entity = std::nullopt;
            return;

        case eng::Key::Delete:
            if (selected_entity.has_value()) {
                scene.destroy_entity(selected_entity.value());
                selected_entity = std::nullopt;
                return;
            }

            break;

        case eng::Key::D:
            if (event.key.ctrl && selected_entity.has_value()) {
                selected_entity = scene.duplicate(selected_entity.value());
                return;
            }

            break;

        case eng::Key::Q:
            gizmo_op = (ImGuizmo::OPERATION)-1;
            return;

        case eng::Key::W:
            gizmo_op = ImGuizmo::TRANSLATE;
            return;

        case eng::Key::E:
            gizmo_op = ImGuizmo::ROTATE;
            return;

        case eng::Key::R:
            gizmo_op = ImGuizmo::SCALE;
            return;

        case eng::Key::LeftShift:
            if (selected_entity.has_value()) {
                eng::Transform &t = selected_entity.value().get_component<eng::Transform>();
                camera.cam_control = eng::OrbitalControl::create(&camera, &t.position);
            }

            return;

        default:
            break;
        }

        break;

    case eng::EventType::KeyReleased:
        switch (event.key.key) {
        case eng::Key::LeftShift:
            camera.cam_control = eng::TrackballControl::create(&camera);
            return;

        default:
            break;
        }

        break;

    case eng::EventType::MouseButtonPressed: {
        if (event.mouse_button.button == eng::MouseButton::Left &&
            viewport_hovered && !lock_focus) {
            glm::vec2 mouse_pos = eng::get_mouse_position();
            glm::vec2 local_mouse_pos = mouse_pos - viewport_pos;
            glm::u8vec4 pixel = main_fbo.pixel_at(local_mouse_pos, 1);

            if (pixel == glm::u8vec4(0)) {
                selected_entity = std::nullopt;
                return;
            }

            uint32_t red_contrib = pixel.r * 65025;
            uint32_t green_contrib = pixel.g * 255;
            uint32_t blue_contrib = pixel.b;
            uint32_t id = red_contrib + green_contrib + blue_contrib;
            selected_entity =
                eng::Entity{.handle = id, .owning_reg = &scene.registry};
        }

        break;
    }

    default:
        break;
    }

    camera.on_event(event);
}

void EditorLayer::on_update(float ts) {
    camera.on_update(ts);
}

void EditorLayer::on_tick(uint32_t tickrate) {}

static void setup_dockspace();
static void render_control_panel(EditorLayer &layer);
static void render_entity_panel(EditorLayer &layer);
static void render_gizmo(EditorLayer &layer);

static void on_shadow_pass(EditorLayer &layer) {
    eng::Scene &scene = layer.scene;

    eng::renderer::shadow_pass_begin(layer.camera.render_data(),
                                     layer.asset_pack);

    eng::ecs::RegistryView rview =
        scene.registry.view<eng::Transform, eng::DirLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::DirLight &light = rview.get<eng::DirLight>(entry);

        eng::renderer::submit_dir_light(transform.rotation, light);
    }

    rview =
        scene.registry.view<eng::Transform, eng::PointLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::PointLight &light = rview.get<eng::PointLight>(entry);

        eng::renderer::submit_point_light(transform.position, light);
    }

    rview =
        scene.registry.view<eng::Transform, eng::SpotLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::SpotLight &light = rview.get<eng::SpotLight>(entry);

        eng::renderer::submit_spot_light(transform, light);
    }

    rview =
        scene.registry.view<eng::Transform, eng::MeshComp, eng::MaterialComp>(
            eng::ecs::exclude<eng::PointLight, eng::DirLight, eng::SpotLight>);
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::MeshComp &mesh = rview.get<eng::MeshComp>(entry);

        eng::renderer::submit_shadow_pass_mesh(transform.to_mat4(), mesh.id);
    }

    eng::renderer::shadow_pass_end();
}

void EditorLayer::on_render() {
    setup_dockspace();

    ImGui::Begin("Control panel");
    render_control_panel(*this);
    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("Viewport", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImVec2 content_reg = ImGui::GetContentRegionAvail();
    ImVec2 content_pos = ImGui::GetWindowPos();
    viewport_pos = glm::vec2(content_pos.x, content_pos.y);
    viewport_hovered = ImGui::IsWindowHovered();

    ImGui::Image((ImTextureID)main_fbo.color_attachments[2].id, content_reg,
                 {0.0f, 1.0f}, {1.0f, 0.0f});
    render_gizmo(*this);
    ImGui::PopStyleVar();
    ImGui::End();

    ImGui::Begin("Entity panel");
    render_entity_panel(*this);
    ImGui::End();

    on_shadow_pass(*this);

    glm::ivec2 avail_region_iv2 = {(int32_t)content_reg.x,
                                   (int32_t)content_reg.y};
    camera.viewport = avail_region_iv2;

    main_fbo.bind();
    main_fbo.resize_everything(avail_region_iv2);
    main_fbo.bind_renderbuffer();
    main_fbo.draw_to_color_attachment(0, 0);
    main_fbo.draw_to_color_attachment(1, 1);
    main_fbo.fill_draw_buffers();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glClearColor(0.33f, 0.33f, 0.33f, 1.0f);

    if (selected_entity.has_value() &&
        selected_entity.value().has_component<eng::MeshComp>()) {

        eng::Entity ent = selected_entity.value();
        eng::Transform &transform = ent.get_component<eng::Transform>();
        eng::MeshComp &mesh_comp = ent.get_component<eng::MeshComp>();

        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilMask(0xFF);
        glDisable(GL_DEPTH_TEST);
        eng::renderer::scene_begin(camera.render_data(), asset_pack);
        eng::renderer::submit_mesh(transform.to_mat4(), mesh_comp.id,
                                   eng::AssetPack::DEFAULT_BASE_MATERIAL,
                                   ent.handle);
        eng::renderer::scene_end();
        glEnable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    main_fbo.clear_color_attachment(1);

    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilMask(0x00);
    eng::renderer::scene_begin(camera.render_data(), asset_pack);

    eng::ecs::RegistryView rview =
        scene.registry.view<eng::Transform, eng::DirLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::DirLight &light = rview.get<eng::DirLight>(entry);

        eng::renderer::submit_dir_light(transform.rotation, light);
    }

    rview =
        scene.registry.view<eng::Transform, eng::PointLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::PointLight &light = rview.get<eng::PointLight>(entry);

        eng::renderer::submit_point_light(transform.position, light);
    }

    rview =
        scene.registry.view<eng::Transform, eng::SpotLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::SpotLight &light = rview.get<eng::SpotLight>(entry);

        eng::renderer::submit_spot_light(transform, light);
    }

    rview =
        scene.registry.view<eng::Transform, eng::MeshComp, eng::MaterialComp>(
            eng::ecs::exclude<eng::PointLight, eng::SpotLight>);
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::MeshComp &mesh = rview.get<eng::MeshComp>(entry);
        eng::MaterialComp &mat = rview.get<eng::MaterialComp>(entry);

        eng::renderer::submit_mesh(transform.to_mat4(), mesh.id, mat.id,
                                   entry.entity_id);
    }

    rview = scene.registry.view<eng::Transform, eng::MeshComp,
                                eng::MaterialComp, eng::PointLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::MeshComp &mesh = rview.get<eng::MeshComp>(entry);
        eng::MaterialComp &mat = rview.get<eng::MaterialComp>(entry);
        eng::PointLight &pl = rview.get<eng::PointLight>(entry);

        eng::renderer::DrawParams params;
        params.color_intensity = pl.intensity;

        eng::renderer::submit_mesh(transform.to_mat4(), mesh.id, mat.id,
                                   entry.entity_id, params);
    }

    rview = scene.registry.view<eng::Transform, eng::MeshComp,
                                eng::MaterialComp, eng::SpotLight>();
    for (eng::ecs::RegistryView::Entry &entry : rview.entity_entries) {
        eng::Transform &transform = rview.get<eng::Transform>(entry);
        eng::MeshComp &mesh = rview.get<eng::MeshComp>(entry);
        eng::MaterialComp &mat = rview.get<eng::MaterialComp>(entry);
        eng::SpotLight &sl = rview.get<eng::SpotLight>(entry);

        eng::renderer::DrawParams params;
        params.color_intensity = sl.intensity;

        eng::renderer::submit_mesh(transform.to_mat4(), mesh.id, mat.id,
                                   entry.entity_id, params);
    }

    eng::renderer::scene_end();

    main_fbo.bind_color_attachment(0);
    main_fbo.draw_to_color_attachment(2, 2);
    GL_CALL(glDrawBuffer(GL_COLOR_ATTACHMENT2));
    eng::renderer::draw_to_screen_quad();

    if (selected_entity.has_value() &&
        selected_entity.value().has_component<eng::MeshComp>()) {

        eng::Entity ent = selected_entity.value();
        eng::Transform transform = ent.get_component<eng::Transform>();
        transform.scale += 0.1f;

        eng::MeshComp &mesh_comp = ent.get_component<eng::MeshComp>();

        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilMask(0x00);
        glDisable(GL_DEPTH_TEST);
        eng::renderer::scene_begin(camera.render_data(), asset_pack);
        eng::renderer::submit_mesh(transform.to_mat4(), mesh_comp.id,
                                   outline_material,
                                   ent.handle);
        eng::renderer::scene_end();
        glEnable(GL_DEPTH_TEST);
    }

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilMask(0xFF);
    main_fbo.unbind();
}

static void setup_dockspace() {
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
            dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow("Control panel", left);
        ImGui::DockBuilderDockWindow("Viewport", dockspace_id);
        ImGui::DockBuilderDockWindow("Entity panel", right);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();

}

static void render_control_panel(EditorLayer &layer) {
    ImGui::Text("%s", layer.scene.name.c_str());
    ImGui::Separator();

    ImGui::Text("Entities");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.1f, 0.1f, 0.1f, 1.0f});

    ImVec2 av_space = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("Entities", {av_space.x, av_space.y / 4.0f});

    for (eng::Entity &ent : layer.scene.entities) {
        ImGui::PushID((int32_t)ent.handle);

        if (ImGui::Selectable(ent.get_component<eng::Name>().name.c_str(),
                              ent.handle ==
                                  layer.selected_entity.value_or({}).handle))
            layer.selected_entity = ent;

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (ImGui::PrettyButton("New entity")) {
        ImVec2 pos = ImGui::GetItemRectMin();
        ImVec2 size = ImGui::GetItemRectSize();

        ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + size.y));
        ImGui::OpenPopup("new_entity_group");
    }

    if (ImGui::BeginPopup("new_entity_group")) {
        if (ImGui::MenuItem("Empty entity")) {
            layer.selected_entity = layer.scene.spawn_entity("Empty entity");
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Plane")) {
            eng::Entity ent = layer.scene.spawn_entity("Plane");
            ent.get_component<eng::Transform>().rotation = {
                glm::half_pi<float>(), 0.0f, 0.0f};
            ent.add_component<eng::MeshComp>().id = eng::AssetPack::QUAD_ID;
            ent.add_component<eng::MaterialComp>().id =
                eng::AssetPack::DEFAULT_BASE_MATERIAL;

            layer.selected_entity = ent;

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Cube")) {
            eng::Entity ent = layer.scene.spawn_entity("Cube");
            ent.add_component<eng::MeshComp>().id = eng::AssetPack::CUBE_ID;
            ent.add_component<eng::MaterialComp>().id =
                eng::AssetPack::DEFAULT_BASE_MATERIAL;

            layer.selected_entity = ent;

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("UV Sphere")) {
            eng::Entity ent = layer.scene.spawn_entity("UV Sphere");
            ent.add_component<eng::MeshComp>().id = eng::AssetPack::SPHERE_ID;
            ent.add_component<eng::MaterialComp>().id =
                eng::AssetPack::DEFAULT_BASE_MATERIAL;

            layer.selected_entity = ent;

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::BeginPrettyCombo(
        "Gizmo mode", layer.gizmo_mode == ImGuizmo::WORLD ? "World" : "Local",
        [&layer]() {
            if (ImGui::Selectable("World", layer.gizmo_mode == ImGuizmo::WORLD))
                layer.gizmo_mode = ImGuizmo::WORLD;

            if (ImGui::Selectable("Local", layer.gizmo_mode == ImGuizmo::LOCAL))
                layer.gizmo_mode = ImGuizmo::LOCAL;
        },
        ImGui::CalcTextSize("Gizmo mode").x);

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        float horizontal_size = ImGui::CalcTextSize("Near clip").x;
        ImGui::Indent(16.0f);
        ImGui::PrettyDragFloat3("Position", &layer.camera.position[0], 0.05f, 0.0f,
                                0.0f, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Exposure", &layer.camera.exposure, 0.01f, 0.0f,
                               FLT_MAX, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Gamma", &layer.camera.gamma, 0.01f, 0.0f, FLT_MAX,
                               "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Near clip", &layer.camera.near_clip, 0.01f, 0.0f,
                               layer.camera.far_clip, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Far clip", &layer.camera.far_clip, 0.01f,
                               layer.camera.near_clip, FLT_MAX, "%.3f",
                               horizontal_size);
        ImGui::Unindent(16.0f);
    }

    if (ImGui::CollapsingHeader("Soft shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
        eng::renderer::SoftShadowProps &props =
            eng::renderer::soft_shadow_props();

        float horizontal_size = ImGui::CalcTextSize("Filter size").x;
        ImGui::Indent(8.0f);
        ImGui::PrettyDragInt("Window size", &props.offsets_tex_size, 2, INT_MAX,
                             horizontal_size);
        ImGui::PrettyDragInt("Filter size", &props.offsets_filter_size, 1,
                             INT_MAX, horizontal_size);
        ImGui::PrettyDragFloat("Radius", &props.offset_radius, 0.05f, 0.0f,
                               FLT_MAX, "%.2f", horizontal_size);
        ImGui::Unindent(8.0f);
    }
}

static void render_entity_panel(EditorLayer &layer) {
    if (!layer.selected_entity.has_value())
        return;

    eng::Entity ent = layer.selected_entity.value();

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

        glm::vec3 rot_degrees = glm::degrees(transform.rotation);
        ImGui::PrettyDragFloat3("Rotation", &rot_degrees[0], 0.05f, 0.0f,
                                0.0f, "%.3f", horizontal_size);
        transform.rotation = glm::radians(rot_degrees);

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
                "Mesh", layer.asset_pack.meshes.at(mesh_comp.id).name.c_str(),
                [&layer, &mesh_comp]() {
                    const std::map<eng::AssetID, eng::Mesh> &meshes =
                        layer.asset_pack.meshes;

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

    ImGui::PushID(3);
    if (ent.has_component<eng::MaterialComp>()) {
        eng::MaterialComp &mat_comp = ent.get_component<eng::MaterialComp>();
        eng::Material &mat = layer.asset_pack.materials.at(mat_comp.id);

        if (ImGui::CollapsingHeader("Material",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            ImGui::BeginPrettyCombo(
                "Material", mat.name.c_str(), [&layer, &mat_comp]() {
                    const std::map<eng::AssetID, eng::Material> &materials =
                        layer.asset_pack.materials;

                    for (const auto &[id, material] : materials) {
                        if (ImGui::Selectable(material.name.c_str(),
                                              id == mat_comp.id))
                            mat_comp.id = id;
                    }
                });

            Shader &shader = layer.asset_pack.shaders.at(mat.shader_id);
            ImGui::BeginPrettyCombo(
                "Shader", shader.name.c_str(), [&layer, &mat]() {
                    const std::map<eng::AssetID, Shader> &shaders =
                        layer.asset_pack.shaders;

                    for (const auto &[id, shader] : shaders) {
                        if (ImGui::Selectable(shader.name.c_str(),
                                              id == mat.shader_id))
                            mat.shader_id = id;
                    }
                });

            float horizontal_size = ImGui::CalcTextSize("Factor").x;
            ImGui::PrettyDragFloat2("Factor", &mat.tiling_factor[0], 0.01f,
                                    0.0f, FLT_MAX, "%.2f", horizontal_size);
            ImGui::PrettyDragFloat2("Offset", &mat.texture_offset[0], 0.01f,
                                    0.0f, 0.0f, "%.2f", horizontal_size);

            const char *avail_tex_group = "available_textures_group";
            static eng::AssetID *selected_id = nullptr;
            static TextureFormat desired_format{};

            Texture *tex = &layer.asset_pack.textures.at(mat.albedo_texture_id);
            if (ImGui::TextureFrame(
                    "##Albedo", (ImTextureID)tex->id,
                    [&tex, &mat]() {
                        ImGui::Text("Diffuse texture");
                        ImGui::Text("%s", tex->name.c_str());
                        ImGui::ColorEdit4("Color", &mat.color[0],
                                          ImGuiColorEditFlags_NoInputs);
                    },
                    96.0f)) {
                selected_id = &mat.albedo_texture_id;
                desired_format = TextureFormat::RGBA8;
                ImGui::OpenPopup(avail_tex_group);
            }

            tex = &layer.asset_pack.textures.at(mat.normal_texture_id);
            if (ImGui::TextureFrame(
                    "##Normal", (ImTextureID)tex->id,
                    [&tex]() {
                        ImGui::Text("Normal texture");
                        ImGui::Text("%s", tex->name.c_str());
                    },
                    96.0f)) {
                selected_id = &mat.normal_texture_id;
                desired_format = TextureFormat::RGB8;
                ImGui::OpenPopup(avail_tex_group);
            }

            tex = &layer.asset_pack.textures.at(mat.roughness_texture_id);
            if (ImGui::TextureFrame(
                    "##Roughness", (ImTextureID)tex->id,
                    [&tex, &mat]() {
                        ImGui::Text("Roughness texture");
                        ImGui::Text("%s", tex->name.c_str());
                        ImGui::PrettyDragFloat(
                            "Roughness", &mat.roughness, 0.01f, 0.0f, 1.0f,
                            "%0.2f", ImGui::CalcTextSize("Roughness").x);
                    },
                    96.0f)) {
                selected_id = &mat.roughness_texture_id;
                desired_format = TextureFormat::R8;
                ImGui::OpenPopup(avail_tex_group);
            }

            tex = &layer.asset_pack.textures.at(mat.metallic_texture_id);
            if (ImGui::TextureFrame(
                    "##Metallic", (ImTextureID)tex->id,
                    [&tex, &mat]() {
                        ImGui::Text("Metallic texture");
                        ImGui::Text("%s", tex->name.c_str());
                        ImGui::PrettyDragFloat(
                            "Metallic", &mat.metallic, 0.01f, 0.0f, 1.0f,
                            "%0.2f", ImGui::CalcTextSize("Roughness").x);
                    },
                    96.0f)) {
                selected_id = &mat.metallic_texture_id;
                desired_format = TextureFormat::R8;
                ImGui::OpenPopup(avail_tex_group);
            }

            tex = &layer.asset_pack.textures.at(mat.ao_texture_id);
            if (ImGui::TextureFrame(
                    "##AO", (ImTextureID)tex->id,
                    [&tex, &mat]() {
                        ImGui::Text("AO texture");
                        ImGui::Text("%s", tex->name.c_str());
                        ImGui::PrettyDragFloat(
                            "AO", &mat.ao, 0.01f, 0.0f, 1.0f,
                            "%0.2f", ImGui::CalcTextSize("Roughness").x);
                    },
                    96.0f)) {
                selected_id = &mat.ao_texture_id;
                desired_format = TextureFormat::R8;
                ImGui::OpenPopup(avail_tex_group);
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f, 10.0f});
            if (ImGui::BeginPopup(avail_tex_group)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {15.0f, 0.0f});

                uint32_t count = 0;
                for (const auto &[id, texture] : layer.asset_pack.textures) {
                    ImGui::PushID(count);

                    if (ImGui::ImageButton(
                            "#Texture", (ImTextureID)(texture.id),
                            {64.0f, 64.0f}, {0.0f, 1.0f}, {1.0f, 0.0f})) {
                        *selected_id = id;
                    }

                    if ((++count) % 3 == 0)
                        ImGui::NewLine();
                    else
                        ImGui::SameLine();

                    ImGui::PopID();
                }

                if (count % 3 != 0)
                    ImGui::NewLine();

                ImGui::NewLine();

                if (ImGui::Button("New texture")) {
                    IGFD::FileDialogConfig config;
                    config.path = ".";
                    config.flags = ImGuiFileDialogFlags_Modal;

                    ImGuiFileDialog::Instance()->OpenDialog(
                        "file_dial_texture", "Choose file", ".jpg,.jpeg,.png",
                        config);
                }

                ImGui::PopStyleVar();
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();

            ImGui::SetNextWindowSize({600.0f, 400.0f}, ImGuiCond_FirstUseEver);
            if (ImGuiFileDialog::Instance()->Display("file_dial_texture")) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    std::string path =
                        ImGuiFileDialog::Instance()->GetFilePathName();

                    Texture new_tex =
                        Texture::create(path, desired_format);
                    *selected_id = layer.asset_pack.add_texture(new_tex);
                }

                ImGuiFileDialog::Instance()->Close();
            }

            if (ImGui::PrettyButton("Remove component"))
                ent.remove_component<eng::MaterialComp>();

            ImGui::Unindent(8.0f);
        }
    }
    ImGui::PopID();

    ImGui::PushID(4);
    if (ent.has_component<eng::DirLight>()) {
        eng::DirLight &dl = ent.get_component<eng::DirLight>();

        if (ImGui::CollapsingHeader("Directional light",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            ImGui::ColorEdit3("Color", &dl.color[0],
                              ImGuiColorEditFlags_NoInputs);

            if (ImGui::PrettyButton("Remove component"))
                ent.remove_component<eng::DirLight>();

            ImGui::Unindent(8.0f);
        }
    }
    ImGui::PopID();

    ImGui::PushID(5);
    if (ent.has_component<eng::PointLight>()) {
        eng::PointLight &pl = ent.get_component<eng::PointLight>();

        if (ImGui::CollapsingHeader("Point light",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);

            ImGui::ColorEdit3("Color", &pl.color[0],
                              ImGuiColorEditFlags_NoInputs);

            float width = ImGui::CalcTextSize("Quadratic").x;
            ImGui::PrettyDragFloat("Intensity", &pl.intensity, 0.01f, 0.0f,
                                   FLT_MAX, "%.2f", width);
            ImGui::PrettyDragFloat("Linear", &pl.linear, 0.01f, 0.0f,
                                   FLT_MAX, "%.2f", width);
            ImGui::PrettyDragFloat("Quadratic", &pl.quadratic, 0.01f, 0.0f,
                                   FLT_MAX, "%.2f", width);

            if (ImGui::PrettyButton("Remove component"))
                ent.remove_component<eng::PointLight>();

            ImGui::Unindent(8.0f);

        }
    }
    ImGui::PopID();

    ImGui::PushID(6);
    if (ent.has_component<eng::SpotLight>()) {
        eng::SpotLight &sl = ent.get_component<eng::SpotLight>();

        if (ImGui::CollapsingHeader("Spot light",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            ImGui::ColorEdit3("Color", &sl.color[0],
                              ImGuiColorEditFlags_NoInputs);
            ImGui::PrettyDragFloat("Intensity", &sl.intensity, 0.001f, 0.0f,
                                   FLT_MAX, "%.3f");
            ImGui::PrettyDragFloat("Cutoff", &sl.cutoff, 0.01f, 0.0f, FLT_MAX,
                                   "%.3f");
            ImGui::PrettyDragFloat("Smoothness", &sl.edge_smoothness, 0.01f,
                                   0.0f, sl.cutoff, "%.3f");
            ImGui::PrettyDragFloat("Linear", &sl.linear, 0.0001f, 0.0f, FLT_MAX,
                                   "%.5f");
            ImGui::PrettyDragFloat("Quadratic", &sl.quadratic, 0.00001f, 0.0f,
                                   FLT_MAX, "%.5f");

            if (ImGui::PrettyButton("Remove component"))
                ent.remove_component<eng::SpotLight>();

            ImGui::Unindent(8.0f);
        }
    }
    ImGui::PopID();

    if (ImGui::PrettyButton("Add component")) {
        ImVec2 pos = ImGui::GetItemRectMin();
        ImVec2 size = ImGui::GetItemRectSize();

        ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + size.y));
        ImGui::OpenPopup("new_comp_group");
    }

#define COMP_ADDER(entity, CompType, label)                                    \
    if (!(entity).has_component<CompType>() && ImGui::MenuItem(label)) {       \
        (entity).add_component<CompType>();                                    \
        ImGui::CloseCurrentPopup();                                            \
    }

#define GEN_COMP_ADDERS(entity)                                                \
    COMP_ADDER(entity, eng::MeshComp, "Mesh")                                  \
    COMP_ADDER(entity, eng::MaterialComp, "Material")                          \
    COMP_ADDER(entity, eng::PointLight, "Point light")                         \
    COMP_ADDER(entity, eng::DirLight, "Directional light")                     \
    COMP_ADDER(entity, eng::SpotLight, "Spot light")

    if (ImGui::BeginPopup("new_comp_group")) {
        GEN_COMP_ADDERS(ent);
        ImGui::EndPopup();
    }
}

static void render_gizmo(EditorLayer &layer) {
    if (!layer.selected_entity.has_value() ||
        layer.gizmo_op == (ImGuizmo::OPERATION)-1) {
        layer.lock_focus = false;
        return;
    }

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetCurrentWindow()->DrawList);

    glm::vec2 &viewport_pos = layer.viewport_pos;
    glm::vec2 &viewport_size = layer.camera.viewport;
    float title_bar_height = ImGui::GetCurrentWindow()->TitleBarHeight;
    ImGuizmo::SetRect(viewport_pos.x, viewport_pos.y + title_bar_height,
                      viewport_size.x, viewport_size.y);

    eng::Entity ent = layer.selected_entity.value();
    const glm::mat4 &camera_proj = layer.camera.projection();
    const glm::mat4 &camera_view = layer.camera.view();
    glm::mat4 transform = ent.get_component<eng::Transform>().to_mat4();

    bool do_snap = eng::is_key_pressed(eng::Key::LeftControl);
    float snap_step = (layer.gizmo_op == ImGuizmo::ROTATE ? 45.0f : 0.5f);
    float snap_vals[3] = {snap_step, snap_step, snap_step};

    ImGuizmo::Manipulate(&camera_view[0][0], &camera_proj[0][0], layer.gizmo_op,
                         layer.gizmo_mode, &transform[0][0], nullptr,
                         (do_snap ? snap_vals : nullptr));

    layer.lock_focus = ImGuizmo::IsOver();

    if (ImGuizmo::IsUsing()) {
        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;
        eng::Transform &t_comp = ent.get_component<eng::Transform>();

        (void)transform_decompose(transform, position, rotation, scale);
        glm::vec3 delta_rot = rotation - t_comp.rotation;

        t_comp.position = position;
        t_comp.rotation = t_comp.rotation + delta_rot;
        t_comp.scale = scale;
    }
}
