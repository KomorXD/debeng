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


    {
        layer->main_fbo = Framebuffer::create();
        layer->main_fbo.add_depth_attachment(
            {.type = DepthAttachmentType::DEPTH_STENCIL,
             .tex_type = TextureType::TEX_2D,
             .size = window_size});

        ColorAttachmentSpec spec;
        spec.type = TextureType::TEX_2D;
        spec.format = TextureFormat::RGBA16F;
        spec.wrap = GL_CLAMP_TO_EDGE;
        spec.min_filter = spec.mag_filter = GL_NEAREST;
        spec.size = window_size;
        spec.gen_minmaps = false;
        layer->main_fbo.add_color_attachment(spec);

        spec.format = TextureFormat::RGBA8;
        layer->main_fbo.add_color_attachment(spec);
        layer->main_fbo.add_color_attachment(spec);

        layer->main_fbo.draw_to_depth_attachment(0);
        layer->main_fbo.draw_to_color_attachment(0, 0);
        assert(layer->main_fbo.is_complete() && "Incomplete main framebuffer");
    }

    layer->asset_pack = eng::AssetPack::create("default");

    layer->scene = eng::Scene::create("New scene");

    eng::Entity ent = layer->scene.spawn_entity("ent");
    ent.get_component<eng::Transform>().scale = glm::vec3(10.0f, 1.0f, 10.0f);
    ent.add_component<eng::MeshComp>().id = eng::AssetPack::CUBE_ID;
    ent.add_component<eng::MaterialComp>().id =
        eng::AssetPack::DEFAULT_BASE_MATERIAL;

    ent = layer->scene.spawn_entity("light");
    ent.get_component<eng::Transform>().position = glm::vec3(0.0f, 4.0f, 0.0f);
    ent.add_component<eng::PointLight>().intensity = 10.0f;
    ent.add_component<eng::MeshComp>().id = eng::AssetPack::CUBE_ID;
    ent.add_component<eng::MaterialComp>().id =
        eng::AssetPack::DEFAULT_FLAT_MATERIAL;

    eng::Material mat;
    mat.name = "Outline";
    mat.color = glm::vec4(0.76f, 0.20f, 0.0f, 1.0f);
    mat.shader_id = eng::AssetPack::DEFAULT_FLAT_MATERIAL;
    layer->outline_material = layer->asset_pack.add_material(mat);

    TextureSpec spec;
    spec.format = TextureFormat::RGBA16F;
    spec.min_filter = spec.mag_filter = GL_LINEAR;
    spec.wrap = GL_REPEAT;
    spec.gen_mipmaps = false;
    Texture thumbnail = Texture::create(
        std::string("resources/textures/envmaps/xdd.hdr"), spec);
    eng::EnvMap env_map = eng::renderer::create_envmap(thumbnail);
    layer->envmap_id = layer->asset_pack.add_env_map(env_map);
    eng::renderer::use_envmap(layer->asset_pack.env_maps.at(layer->envmap_id));

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

    eng::renderer::reset_stats();
    on_shadow_pass(*this);

    glm::ivec2 avail_region_iv2 = {(int32_t)content_reg.x,
                                   (int32_t)content_reg.y};
    camera.viewport = avail_region_iv2;

    main_fbo.bind();
    main_fbo.resize_everything(avail_region_iv2);
    main_fbo.draw_to_depth_attachment(0);
    main_fbo.draw_to_color_attachment(0, 0);
    main_fbo.draw_to_color_attachment(1, 1);
    main_fbo.fill_color_draw_buffers();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glClearColor(0.33f, 0.33f, 0.33f, 1.0f);
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
    eng::renderer::skybox(envmap_id);

    main_fbo.bind_color_attachment(0);
    eng::renderer::post_process();

    main_fbo.bind_color_attachment_image(2, 0, 2, ImageAccess::WRITE);
    GL_CALL(glDrawBuffer(GL_COLOR_ATTACHMENT2));
    eng::renderer::post_proc_combine();

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

    if (ImGui::CollapsingHeader("Environment"),
        ImGuiTreeNodeFlags_DefaultOpen) {
        eng::EnvMap &envmap = layer.asset_pack.env_maps.at(layer.envmap_id);

        ImGui::Indent(8.0f);
        if (ImGui::TextureFrame(
                "##Envmap", (ImTextureID)envmap.thumbnail.id,
                [&envmap]() {
                    ImGui::Text("Env map");
                    ImGui::Text("%s", envmap.thumbnail.name.c_str());
                },
                96.0f)) {
            ImGui::OpenPopup("avail_envmaps_group");
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f, 10.0f});
        if (ImGui::BeginPopup("avail_envmaps_group")) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {15.0f, 0.0f});

            int32_t count = 0;
            for (const auto &[id, envmap] : layer.asset_pack.env_maps) {
                ImGui::PushID(count);

                if (ImGui::ImageButton(
                        "#Envmap", (ImTextureID)envmap.thumbnail.id,
                        {64.0f, 64.0f}, {0.0f, 1.0f}, {1.0f, 0.0f})) {
                    layer.envmap_id = id;
                    eng::renderer::use_envmap(layer.asset_pack.env_maps.at(id));
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

            if (ImGui::Button("New envmap")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                config.flags = ImGuiFileDialogFlags_Modal;

                ImGuiFileDialog::Instance()->OpenDialog(
                    "file_dial_envmap", "Choose file", ".hdr", config);
            }

            ImGui::PopStyleVar();
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
    }

    ImGui::SetNextWindowSize({600.0f, 400.0f}, ImGuiCond_FirstUseEver);
    if (ImGuiFileDialog::Instance()->Display("file_dial_envmap")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();

            TextureSpec spec;
            spec.format = TextureFormat::RGBA16F;
            spec.min_filter = spec.mag_filter = GL_LINEAR;
            spec.wrap = GL_REPEAT;
            spec.gen_mipmaps = false;

            Texture equirect = Texture::create(path, spec);
            eng::EnvMap env_map = eng::renderer::create_envmap(equirect);
            layer.envmap_id = layer.asset_pack.add_env_map(env_map);
            eng::renderer::use_envmap(
                layer.asset_pack.env_maps.at(layer.envmap_id));
        }

        ImGuiFileDialog::Instance()->Close();
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
        float horizontal_size = ImGui::CalcTextSize("Bloom threshold").x;
        ImGui::Indent(8.0f);
        ImGui::PrettyDragFloat3("Position", &layer.camera.position[0], 0.05f,
                                0.0f, 0.0f, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Exposure", &layer.camera.exposure, 0.01f, 0.0f,
                               FLT_MAX, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Gamma", &layer.camera.gamma, 0.01f, 0.0f,
                               FLT_MAX, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Bloom strength", &layer.camera.bloom_strength,
                               0.01f, 0.0f, FLT_MAX, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Bloom threshold", &layer.camera.bloom_threshold,
                               0.01f, 0.0f, FLT_MAX, "%.3f", horizontal_size);
        ImGui::PrettyDragInt("Bloom mip radius", &layer.camera.bloom_mip_radius,
                             1, 7, horizontal_size);
        ImGui::PrettyDragFloat("Near clip", &layer.camera.near_clip, 0.01f,
                               0.0f, layer.camera.far_clip, "%.3f",
                               horizontal_size);
        ImGui::PrettyDragFloat("Far clip", &layer.camera.far_clip, 0.01f,
                               layer.camera.near_clip, FLT_MAX, "%.3f",
                               horizontal_size);
        ImGui::Unindent(8.0f);
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

    if (ImGui::CollapsingHeader("Render stats")) {
        eng::renderer::RenderStats stats = eng::renderer::stats();

        float horizontal_size = ImGui::CalcTextSize("Shadow pass (ms)").x;
        ImGui::Indent(8.0f);

        if (ImGui::BeginTable("#Stats", 2)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                    horizontal_size);
            ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch);

            {
                std::array<float, 2> values = {stats.base_pass_ms,
                                               stats.shadow_pass_ms};
                std::array<const char *, 2> labels = {"Color pass",
                                                      "Shadow pass"};

                for (int32_t i = 0; i < labels.size(); i++) {
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%s", labels[i]);
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%.3fms", values[i]);

                    ImGui::TableNextRow();
                }
            }

            {
                /* Lights divided by 2 because they're submitted for shadow AND
                 * base pass. */
                std::array<int32_t, 5> values = {
                    stats.dir_lights / 2, stats.point_lights / 2,
                    stats.spot_lights / 2, stats.instances, stats.draw_calls};
                std::array<const char *, 5> labels = {
                    "Dir lights", "Point lights", "Spot lights", "Instances",
                    "Draw calls"};

                for (int32_t i = 0; i < labels.size(); i++) {
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%s", labels[i]);
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%d", values[i]);

                    ImGui::TableNextRow();
                }
            }

            ImGui::EndTable();
        }

        ImGui::Unindent(8.0f);
    }
}

bool material_texture_widget(const char *label, Texture &texture,
                             float label_width) {
    bool pressed = false;

    if (ImGui::BeginTable("#Texture", 2)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                label_width);
        ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        if (ImGui::Selectable(texture.name.c_str(), true))
            pressed = true;

        ImGui::PopStyleColor(3);

        ImDrawList *dlist = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        dlist->AddRect(min, max, IM_COL32(32, 32, 32, 255));

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::Image((ImTextureID)texture.id, {96.0f, 96.0f}, {0.0f, 1.0f},
                         {1.0f, 0.0f});
            ImGui::EndTooltip();
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();

        {
            TextureSpec spec = texture.spec;

            ImGui::BeginPrettyCombo(
                "Filtering", texture.filter_str(), [&texture, &spec]() {
                    if (ImGui::Selectable("Point", texture.spec.mag_filter ==
                                                       GL_NEAREST)) {
                        spec.mag_filter = GL_NEAREST;
                        spec.min_filter =
                            (texture.has_mips() ? GL_NEAREST_MIPMAP_NEAREST
                                                : GL_NEAREST);
                    }

                    if (ImGui::Selectable(
                            "Bilinear", texture.spec.min_filter == GL_LINEAR ||
                                            texture.spec.min_filter ==
                                                GL_LINEAR_MIPMAP_NEAREST)) {
                        spec.mag_filter = GL_LINEAR;
                        spec.min_filter =
                            (texture.has_mips() ? GL_LINEAR_MIPMAP_NEAREST
                                                : GL_LINEAR);
                    }

                    if (ImGui::Selectable("Trilinear",
                                          texture.spec.min_filter ==
                                              GL_LINEAR_MIPMAP_LINEAR)) {
                        spec.mag_filter = GL_LINEAR;
                        spec.min_filter =
                            (texture.has_mips() ? GL_LINEAR_MIPMAP_LINEAR
                                                : GL_LINEAR);
                    }
                });

            ImGui::BeginPrettyCombo(
                "Wrap", texture.wrap_str(), [&texture, &spec]() {
                    GLint modes[] = {GL_REPEAT, GL_MIRRORED_REPEAT,
                                     GL_CLAMP_TO_EDGE, GL_MIRROR_CLAMP_TO_EDGE,
                                     GL_CLAMP_TO_BORDER};
                    constexpr int32_t MODES_SIZE = IM_ARRAYSIZE(modes);
                    for (int32_t i = 0; i < MODES_SIZE; i++) {
                        if (ImGui::Selectable(Texture::wrap_str(modes[i]),
                                              texture.spec.wrap == modes[i]))
                            spec.wrap = modes[i];
                    }
                });

            texture.change_params(spec);
        }

        ImGui::EndTable();
    }

    return pressed;
}

static void render_entity_panel(EditorLayer &layer) {
    if (!layer.selected_entity.has_value())
        return;

    eng::Entity ent = layer.selected_entity.value();

    eng::Name &name = ent.get_component<eng::Name>();
    char buf[128] = { 0 };
    strncpy(buf, name.name.data(), 128);

    float horizontal_size = ImGui::CalcTextSize("Name").x;
    ImGui::Indent(8.0f);
    ImGui::PrettyInputText("Name", buf, horizontal_size);
    name.name = buf;

    ImGui::Unindent(8.0f);

    eng::Transform &transform = ent.get_component<eng::Transform>();
    ImGui::PushID(1);
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        horizontal_size = ImGui::CalcTextSize("Position").x;
        ImGui::Indent(8.0f);
        ImGui::PrettyDragFloat3("Position", &transform.position[0], 0.05f, 0.0f,
                                0.0f, "%.3f", horizontal_size);

        glm::vec3 rot_degrees = glm::degrees(transform.rotation);
        ImGui::PrettyDragFloat3("Rotation", &rot_degrees[0], 0.05f, 0.0f,
                                0.0f, "%.3f", horizontal_size);
        transform.rotation = glm::radians(rot_degrees);

        ImGui::PrettyDragFloat3("Scale", &transform.scale[0], 0.05f, 0.0f, 0.0f,
                                "%.3f", horizontal_size);
        ImGui::Unindent(8.0f);
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

            if (ImGui::PrettyButton("New material")) {
                eng::Material new_mat;
                new_mat.name = "New material";
                new_mat.shader_id = eng::AssetPack::DEFAULT_BASE_MATERIAL;

                mat_comp.id = layer.asset_pack.add_material(new_mat);
            }

            if (mat_comp.id > eng::AssetPack::DEFAULT_FLAT_MATERIAL &&
                mat_comp.id != layer.outline_material) {
                ImGui::SameLine();

                if (ImGui::PrettyButton("Delete material")) {
                    layer.asset_pack.materials.erase(mat_comp.id);

                    eng::ecs::RegistryView rview =
                        layer.scene.registry.view<eng::MaterialComp>();
                    for (eng::ecs::RegistryView::Entry &entry :
                         rview.entity_entries) {
                        eng::MaterialComp &comp =
                            rview.get<eng::MaterialComp>(entry);

                        if (comp.id == mat_comp.id)
                            comp.id = eng::AssetPack::DEFAULT_BASE_MATERIAL;
                    }
                }
            }

            float horizontal_size = ImGui::CalcTextSize("Roughness").x;

            ImGui::BeginPrettyCombo(
                "Material", mat.name.c_str(), [&layer, &mat_comp]() {
                    const std::map<eng::AssetID, eng::Material> &materials =
                        layer.asset_pack.materials;

                    for (const auto &[id, material] : materials) {
                        if (ImGui::Selectable(material.name.c_str(),
                                              id == mat_comp.id))
                            mat_comp.id = id;
                    }
                }, horizontal_size);

            if (mat_comp.id > eng::AssetPack::DEFAULT_FLAT_MATERIAL &&
                mat_comp.id != layer.outline_material) {
                char buf[128] = {0};
                strncpy(buf, mat.name.c_str(), 128);
                ImGui::PrettyInputText("Name", buf, horizontal_size);
                mat.name = buf;
            }

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
                }, horizontal_size);

            ImGui::PrettyDragFloat2("Factor", &mat.tiling_factor[0], 0.01f,
                                    0.0f, FLT_MAX, "%.2f", horizontal_size);
            ImGui::PrettyDragFloat2("Offset", &mat.texture_offset[0], 0.01f,
                                    0.0f, 0.0f, "%.2f", horizontal_size);

            ImGui::PrettyColorEdit4("Color", &mat.color[0], horizontal_size);
            ImGui::PrettyDragFloat("Roughness", &mat.roughness, 0.005f, 0.0f,
                                   1.0f, "%.3f", horizontal_size);
            ImGui::PrettyDragFloat("Metallic", &mat.metallic, 0.005f, 0.0f,
                                   1.0f, "%.3f", horizontal_size);
            ImGui::PrettyDragFloat("AO", &mat.ao, 0.005f, 0.0f, 1.0f, "%.3f",
                                   horizontal_size);

            if (ImGui::CollapsingHeader("Textures",
                                        ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(8.0f);

                const char *avail_tex_group = "available_textures_group";
                static eng::AssetID *selected_id = nullptr;
                static TextureFormat desired_format{};

                eng::AssetID *tex_ids[] = {
                    &mat.albedo_texture_id, &mat.normal_texture_id,
                    &mat.roughness_texture_id, &mat.metallic_texture_id,
                    &mat.ao_texture_id};
                TextureFormat formats[] = {
                    TextureFormat::RGBA8, TextureFormat::RGB8,
                    TextureFormat::R8, TextureFormat::R8, TextureFormat::R8};
                const char *labels[] = {"Albedo", "Normal", "Roughness",
                                        "Metallic", "AO"};

                constexpr int32_t TEXTURES = IM_ARRAYSIZE(tex_ids);
                horizontal_size = ImGui::CalcTextSize("Roughness").x + 1.0f;

                for (int32_t i = 0; i < TEXTURES; i++) {
                    Texture &tex = layer.asset_pack.textures.at(*tex_ids[i]);
                    if (material_texture_widget(labels[i], tex,
                                                horizontal_size)) {
                        selected_id = tex_ids[i];
                        desired_format = formats[i];
                        ImGui::OpenPopup(avail_tex_group);
                    }
                }

                ImGui::Unindent(8.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                    {10.0f, 10.0f});
                if (ImGui::BeginPopup(avail_tex_group)) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                        {15.0f, 0.0f});

                    uint32_t count = 0;
                    for (const auto &[id, texture] :
                         layer.asset_pack.textures) {
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
                            "file_dial_texture", "Choose file",
                            ".jpg,.jpeg,.png", config);
                    }

                    ImGui::PopStyleVar();
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();

                ImGui::SetNextWindowSize({600.0f, 400.0f},
                                         ImGuiCond_FirstUseEver);
                if (ImGuiFileDialog::Instance()->Display("file_dial_texture")) {
                    if (ImGuiFileDialog::Instance()->IsOk()) {
                        std::string path =
                            ImGuiFileDialog::Instance()->GetFilePathName();

                        TextureSpec spec;
                        spec.format = desired_format;
                        spec.min_filter = GL_LINEAR_MIPMAP_LINEAR;
                        spec.mag_filter = GL_LINEAR;
                        spec.wrap = GL_REPEAT;
                        spec.gen_mipmaps = true;

                        Texture new_tex = Texture::create(path, spec);
                        *selected_id = layer.asset_pack.add_texture(new_tex);
                    }

                    ImGuiFileDialog::Instance()->Close();
                }
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
            ImGui::PrettyDragFloat("Intensity", &dl.intensity, 0.01f, 0.0f,
                                   FLT_MAX, "%.2f",
                                   ImGui::CalcTextSize("Intensity").x);

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
            ImGui::PrettyDragFloat("Linear", &pl.linear, 0.0001f, 0.0f,
                                   FLT_MAX, "%.5f", width);
            ImGui::PrettyDragFloat("Quadratic", &pl.quadratic, 0.0001f, 0.0f,
                                   FLT_MAX, "%.5f", width);

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
            ImGui::PrettyDragFloat("Quadratic", &sl.quadratic, 0.0001f, 0.0f,
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
