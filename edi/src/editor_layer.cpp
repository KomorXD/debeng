#include "eng/containers/registry.hpp"
#include "eng/event.hpp"
#include "eng/input.hpp"
#include "eng/renderer/camera.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/renderer/renderer.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include "glm/fwd.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "layers.hpp"
#include <cfloat>

std::unique_ptr<Layer> EditorLayer::create(const eng::WindowSpec &win_spec) {
    glm::ivec2 window_size = glm::ivec2(win_spec.width, win_spec.height);

    std::unique_ptr<EditorLayer> layer = std::make_unique<EditorLayer>();
    layer->camera.position = glm::vec3(0.0f, 2.0f, -3.0f);
    layer->camera.viewport = window_size;
    layer->camera.control_mode = eng::SpectatorCamera::ControlMode::FPS;
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

    eng::Entity ent = layer->scene.spawn_entity("ent1");
    ent.get_component<eng::Transform>().position = glm::vec3(2.0f, 0.0f, 0.0f);
    ent.add_component<eng::MeshComp>().id = eng::AssetPack::CUBE_ID;
    ent.add_component<eng::MaterialComp>().id =
        eng::AssetPack::DEFAULT_MATERIAL;

    layer->selected_entity = ent;

    ent = layer->scene.spawn_entity("light");
    ent.get_component<eng::Transform>().position = glm::vec3(2.0f, 2.0f, 1.0f);
    ent.add_component<eng::PointLight>().intensity = 3.0f;

    eng::Material mat;
    mat.name = "Outline";
    mat.color = glm::vec4(0.76f, 0.20f, 0.0f, 1.0f);
    layer->outline_material = layer->asset_pack.add_material(mat);

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

    case eng::EventType::MouseButtonPressed: {
        if (event.mouse_button.button == eng::MouseButton::Left &&
            viewport_hovered) {
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

    case eng::EventType::MouseWheelScrolled:
        camera.scroll_update(event.mouse_scroll.offset_y);
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

        ImGui::DockBuilderDockWindow("Control panel", left);
        ImGui::DockBuilderDockWindow("dock_main", dockspace_id);
        ImGui::DockBuilderDockWindow("Entity panel", right);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();

    ImGui::Begin("Control panel");
    render_control_panel();
    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("dock_main", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImVec2 content_reg = ImGui::GetContentRegionAvail();
    ImVec2 content_pos = ImGui::GetWindowPos();
    viewport_pos = glm::vec2(content_pos.x, content_pos.y);
    viewport_hovered = ImGui::IsWindowHovered();

    ImGui::Image((ImTextureID)main_fbo.color_attachments[2].id, content_reg,
                 {0.0f, 1.0f}, {1.0f, 0.0f});
    ImGui::PopStyleVar();
    ImGui::End();

    ImGui::Begin("Entity panel");
    render_entity_panel();
    ImGui::End();

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
        eng::renderer::RenderPassMode orig_mode = eng::renderer::render_mode();

        eng::Entity ent = selected_entity.value();
        eng::Transform &transform = ent.get_component<eng::Transform>();
        eng::MeshComp &mesh_comp = ent.get_component<eng::MeshComp>();

        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilMask(0xFF);
        glDisable(GL_DEPTH_TEST);
        eng::renderer::set_render_mode(eng::renderer::RenderPassMode::FLAT);
        eng::renderer::scene_begin(camera.camera_render_data(), asset_pack);
        eng::renderer::submit_mesh(transform.to_mat4(), mesh_comp.id,
                                   eng::AssetPack::DEFAULT_MATERIAL,
                                   ent.handle);
        eng::renderer::scene_end();
        eng::renderer::set_render_mode(orig_mode);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    main_fbo.clear_color_attachment(1);

    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilMask(0x00);
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

        eng::renderer::submit_mesh(transform.to_mat4(), mesh.id, mat.id,
                                   entry.entity_id);
    }

    eng::renderer::scene_end();

    main_fbo.bind_color_attachment(0);
    main_fbo.draw_to_color_attachment(2, 2);
    GL_CALL(glDrawBuffer(GL_COLOR_ATTACHMENT2));
    eng::renderer::draw_to_screen_quad();

    if (selected_entity.has_value() &&
        selected_entity.value().has_component<eng::MeshComp>()) {
        eng::renderer::RenderPassMode orig_mode = eng::renderer::render_mode();

        eng::Entity ent = selected_entity.value();
        eng::Transform transform = ent.get_component<eng::Transform>();
        transform.scale += 0.1f;

        eng::MeshComp &mesh_comp = ent.get_component<eng::MeshComp>();

        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilMask(0x00);
        glDisable(GL_DEPTH_TEST);
        eng::renderer::set_render_mode(eng::renderer::RenderPassMode::FLAT);
        eng::renderer::scene_begin(camera.camera_render_data(), asset_pack);
        eng::renderer::submit_mesh(transform.to_mat4(), mesh_comp.id,
                                   outline_material,
                                   ent.handle);
        eng::renderer::scene_end();
        eng::renderer::set_render_mode(orig_mode);
        glEnable(GL_DEPTH_TEST);
    }

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilMask(0xFF);
    main_fbo.unbind();
}

void EditorLayer::render_control_panel() {
    ImGui::Text("%s", scene.name.c_str());
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        float horizontal_size = ImGui::CalcTextSize("Near clip").x;
        ImGui::Indent(16.0f);
        ImGui::PrettyDragFloat3("Position", &camera.position[0], 0.05f, 0.0f,
                                0.0f, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Exposure", &camera.exposure, 0.01f, 0.0f,
                               FLT_MAX, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Gamma", &camera.gamma, 0.01f, 0.0f, FLT_MAX,
                               "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Near clip", &camera.near_clip, 0.01f, 0.0f,
                               camera.far_clip, "%.3f", horizontal_size);
        ImGui::PrettyDragFloat("Far clip", &camera.far_clip, 0.01f,
                               camera.near_clip, FLT_MAX, "%.3f",
                               horizontal_size);
        ImGui::Unindent(16.0f);
    }
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

    ImGui::PushID(3);
    if (ent.has_component<eng::MaterialComp>()) {
        eng::MaterialComp &mat_comp = ent.get_component<eng::MaterialComp>();
        eng::Material &mat = asset_pack.materials.at(mat_comp.id);

        if (ImGui::CollapsingHeader("Material",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(8.0f);
            ImGui::BeginPrettyCombo(
                "Material", mat.name.c_str(), [this, &mat_comp]() {
                    const std::map<eng::AssetID, eng::Material> &materials =
                        asset_pack.materials;

                    for (const auto &[id, material] : materials) {
                        if (ImGui::Selectable(material.name.c_str(),
                                              id == mat_comp.id))
                            mat_comp.id = id;
                    }
                });

            float horizontal_size = ImGui::CalcTextSize("Factor").x;
            ImGui::PrettyDragFloat2("Factor", &mat.tiling_factor[0], 0.01f,
                                    0.0f, FLT_MAX, "%.2f", horizontal_size);
            ImGui::PrettyDragFloat2("Offset", &mat.texture_offset[0], 0.01f,
                                    0.0f, 0.0f, "%.2f", horizontal_size);

            const char *avail_tex_group = "available_textures_group";
            static eng::AssetID *selected_id = nullptr;

            Texture *tex = &asset_pack.textures.at(mat.albedo_texture_id);
            if (ImGui::TextureFrame(
                    "##Albedo", (ImTextureID)tex->id,
                    [&tex, &mat]() {
                        ImGui::Text("Diffuse texture");
                        ImGui::Text("%s", tex->name.c_str());
                        ImGui::PrettyDragFloat4("Color", &mat.color[0], 0.01f,
                                                0.0f, 1.0f, "%.2f",
                                                ImGui::CalcTextSize("Color").x);
                    },
                    96.0f)) {
                selected_id = &mat.albedo_texture_id;
                ImGui::OpenPopup(avail_tex_group);
            }

            tex = &asset_pack.textures.at(mat.normal_texture_id);
            if (ImGui::TextureFrame(
                    "##Normal", (ImTextureID)tex->id,
                    [&tex]() {
                        ImGui::Text("Normal texture");
                        ImGui::Text("%s", tex->name.c_str());
                    },
                    96.0f)) {
                selected_id = &mat.normal_texture_id;
                ImGui::OpenPopup(avail_tex_group);
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f, 10.0f});
            if (ImGui::BeginPopup(avail_tex_group)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {15.0f, 0.0f});

                uint32_t count = 0;
                for (const auto &[id, texture] : asset_pack.textures) {
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

                ImGui::PopStyleVar();
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();

            if (ImGui::PrettyButton("Remove component"))
                ent.remove_component<eng::MaterialComp>();

            ImGui::Unindent(8.0f);
        }
    }
    ImGui::PopID();
}
