#include "eng/renderer/renderer.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include "GLFW/glfw3.h"
#include "glm/fwd.hpp"

#include <algorithm>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace eng::renderer {

struct GPU {
    char *vendor;
    char *opengl_version;
    char *device_name;
    int32_t max_geom_invocations;

    GLint texture_units = 0;
};

using InstancesMap = std::unordered_map<AssetID, std::vector<MeshInstance>>;
using ShaderGroupedInstances = std::unordered_map<AssetID, InstancesMap>;

struct Renderer {
    GPU gpu;

    VertexArray screen_quad_vao;
    Shader screen_quad_shader;

    UniformBuffer camera_uni_buffer;

    UniformBuffer point_lights_uni_buffer;
    std::vector<PointLightData> point_lights;

    UniformBuffer dir_lights_uni_buffer;
    std::vector<DirLightData> dir_lights;

    UniformBuffer spot_lights_uni_buffer;
    std::vector<SpotLightData> spot_lights;

    Framebuffer shadow_fbo;
    Shader spotlight_shadow_shader;
    Shader pointlight_shadow_shader;

    UniformBuffer material_uni_buffer;
    std::vector<AssetID> material_ids;

    std::vector<AssetID> texture_ids;

    ShaderGroupedInstances instances;
};

static Renderer s_renderer{};
static AssetPack *s_asset_pack{};

void opengl_msg_cb(unsigned source, unsigned type, unsigned id,
                   unsigned severity, int length, const char *msg,
                   const void *user_param) {
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
    case GL_DEBUG_SEVERITY_MEDIUM:
    case GL_DEBUG_SEVERITY_LOW:
        fprintf(stderr, "%s\r\n", msg);
        return;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        printf("%s\r\n", msg);
        return;
    }
}

bool init() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    GPU &gpu_spec = s_renderer.gpu;
    GL_CALL(gpu_spec.vendor = (char *)glGetString(GL_VENDOR));
    GL_CALL(gpu_spec.device_name = (char *)glGetString(GL_RENDERER));
    GL_CALL(gpu_spec.opengl_version = (char *)glGetString(GL_VERSION));
    GL_CALL(glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                          &gpu_spec.texture_units));
    GL_CALL(glGetIntegerv(GL_MAX_GEOMETRY_SHADER_INVOCATIONS,
                          &gpu_spec.max_geom_invocations));

    printf("GPU Vendor: %s\n", gpu_spec.vendor);
    printf("GPU Device: %s\n", gpu_spec.device_name);
    printf("OpenGL version: %s\n", gpu_spec.opengl_version);
    printf("Max texture units: %d\n", gpu_spec.texture_units);
    printf("Max geom shader invocs: %d\n", gpu_spec.max_geom_invocations);

    GL_CALL(glEnable(GL_DEBUG_OUTPUT));
    GL_CALL(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
    GL_CALL(glDebugMessageCallback(opengl_msg_cb, nullptr));
    GL_CALL(glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                  GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                                  GL_FALSE));

    GL_CALL(glEnable(GL_DEPTH_TEST));
    GL_CALL(glDepthFunc(GL_LESS));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL_CALL(glEnable(GL_CULL_FACE));
    GL_CALL(glCullFace(GL_BACK));
    GL_CALL(glEnable(GL_LINE_SMOOTH));
    GL_CALL(glEnable(GL_MULTISAMPLE));
    GL_CALL(glEnable(GL_STENCIL_TEST));
    GL_CALL(glStencilFunc(GL_NOTEQUAL, 1, 0xFF));
    GL_CALL(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
    GL_CALL(glStencilMask(0x00));
    GL_CALL(glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS));

    float screen_quad_vertices[] = {
    //    position      texture_uv
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,

         1.0f,  1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f
    };

    s_renderer.screen_quad_vao = VertexArray::create();
    s_renderer.screen_quad_vao.bind();

    VertexBuffer vbo = VertexBuffer::create();
    vbo.allocate(screen_quad_vertices, sizeof(screen_quad_vertices));

    VertexBufferLayout layout;
    layout.push_float(2); // 0 - position
    layout.push_float(2); // 1 - texture UV
    s_renderer.screen_quad_vao.add_vertex_buffer(vbo, layout);

    s_renderer.screen_quad_shader = Shader::create();

    {
        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/screen_quad.vert";
        spec.fragment_shader.path = "resources/shaders/screen_quad.frag";
        spec.fragment_shader.replacements = {
            {
                "${CAMERA_BINDING}",
                std::to_string(CAMERA_BINDING)
            }
        };

        assert(s_renderer.screen_quad_shader.build(spec) &&
               "Screen quad shader not found");
    }

    s_renderer.screen_quad_shader.bind();
    s_renderer.screen_quad_shader.set_uniform_1i("u_screen_texture", 0);

    uint32_t size = sizeof(CameraData);
    s_renderer.camera_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.camera_uni_buffer.bind_buffer_range(CAMERA_BINDING, 0, size);

    size = MAX_POINT_LIGHTS * sizeof(PointLightData) + sizeof(int32_t);
    s_renderer.point_lights_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.point_lights_uni_buffer.bind_buffer_range(POINT_LIGHTS_BINDING,
                                                         0, size);

    size = MAX_DIR_LIGHTS * sizeof(DirLightData) + sizeof(int32_t);
    s_renderer.dir_lights_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.dir_lights_uni_buffer.bind_buffer_range(DIR_LIGHTS_BINDING, 0,
                                                       size);

    size = MAX_SPOT_LIGHTS * sizeof(SpotLightData) + sizeof(int32_t);
    s_renderer.spot_lights_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.spot_lights_uni_buffer.bind_buffer_range(SPOT_LIGHTS_BINDING, 0,
                                                        size);

    {
        ColorAttachmentSpec spec;
        spec.type = ColorAttachmentType::TEX_2D_ARRAY_SHADOW;
        spec.format = TextureFormat::DEPTH_32F;
        spec.wrap = GL_CLAMP_TO_BORDER;
        spec.min_filter = spec.mag_filter = GL_NEAREST;
        spec.size = {512, 512};
        spec.layers = 3;
        spec.gen_minmaps = false;
        spec.border_color = glm::vec4(1.0f);

        s_renderer.shadow_fbo = Framebuffer::create();
        s_renderer.shadow_fbo.bind();
        s_renderer.shadow_fbo.add_color_attachment(spec);

        spec.layers = 16 * 6;
        s_renderer.shadow_fbo.add_color_attachment(spec);
    }

    {
        s_renderer.spotlight_shadow_shader = Shader::create();

        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/depth.vert";
        spec.fragment_shader.path = "resources/shaders/depth.frag";
        spec.geometry_shader = {
            .path = "resources/shaders/shadows/spotlight.geom",
            .replacements = {
                {
                    "${SPOT_LIGHTS_BINDING}",
                    std::to_string(renderer::SPOT_LIGHTS_BINDING)
                },
                {
                    "${MAX_SPOT_LIGHTS}",
                    std::to_string(renderer::MAX_SPOT_LIGHTS)
                },
                {
                    "${INVOCATIONS}",
                    std::to_string(s_renderer.gpu.max_geom_invocations)
                }
            }
        };

        assert(s_renderer.spotlight_shadow_shader.build(spec) &&
               "Spotlight shadow shader not built");
    }

    {
        s_renderer.pointlight_shadow_shader = Shader::create();

        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/depth.vert";
        spec.fragment_shader.path = "resources/shaders/depth.frag";
        spec.geometry_shader = {
            .path = "resources/shaders/shadows/pointlight.geom",
            .replacements = {
                {
                    "${POINT_LIGHTS_BINDING}",
                    std::to_string(renderer::POINT_LIGHTS_BINDING)
                },
                {
                    "${MAX_POINT_LIGHTS}",
                    std::to_string(renderer::MAX_POINT_LIGHTS)
                },
                {
                    "${INVOCATIONS}",
                    std::to_string(s_renderer.gpu.max_geom_invocations)
                }
            }
        };

        assert(s_renderer.pointlight_shadow_shader.build(spec) &&
               "Pointlight shadow shader not built");
    }

    size = MAX_MATERIALS * sizeof(MaterialData);
    s_renderer.material_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.material_uni_buffer.bind_buffer_range(MATERIALS_BINDING, 0,
                                                     size);

    return true;
}

void shutdown() {
    s_renderer.camera_uni_buffer.destroy();
    s_renderer.point_lights_uni_buffer.destroy();
    s_renderer.dir_lights_uni_buffer.destroy();
    s_renderer.spot_lights_uni_buffer.destroy();
    s_renderer.material_uni_buffer.destroy();

    s_renderer.shadow_fbo.destroy();
    s_renderer.spotlight_shadow_shader.destroy();
    s_renderer.pointlight_shadow_shader.destroy();
}

void scene_begin(const CameraData &camera, AssetPack &asset_pack) {
    s_asset_pack = &asset_pack;

    assert(s_asset_pack && "Empty asset pack object");

    s_renderer.point_lights.clear();
    s_renderer.dir_lights.clear();
    s_renderer.spot_lights.clear();
    s_renderer.material_ids.clear();
    s_renderer.texture_ids.clear();

    for (auto &[shader_id, instance_map] : s_renderer.instances) {
        for (auto &[mesh_id, instances] : instance_map)
            instances.clear();

        instance_map.clear();
    }

    s_renderer.instances.clear();

    s_renderer.camera_uni_buffer.bind();
    s_renderer.camera_uni_buffer.set_data(&camera, sizeof(CameraData));
}

void scene_end() {
    s_renderer.point_lights_uni_buffer.bind();
    s_renderer.dir_lights_uni_buffer.bind();
    s_renderer.spot_lights_uni_buffer.bind();

    int32_t count = s_renderer.point_lights.size();
    int32_t offset = MAX_POINT_LIGHTS * sizeof(PointLightData);
    s_renderer.point_lights_uni_buffer.set_data(s_renderer.point_lights.data(),
                                                count * sizeof(PointLightData));
    s_renderer.point_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    count = s_renderer.dir_lights.size();
    offset = MAX_DIR_LIGHTS * sizeof(DirLightData);
    s_renderer.dir_lights_uni_buffer.set_data(s_renderer.dir_lights.data(),
                                                count * sizeof(DirLightData));
    s_renderer.dir_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    count = s_renderer.spot_lights.size();
    offset = MAX_SPOT_LIGHTS * sizeof(SpotLightData);
    s_renderer.spot_lights_uni_buffer.set_data(s_renderer.spot_lights.data(),
                                               count * sizeof(SpotLightData));
    s_renderer.spot_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    std::vector<MaterialData> materials;
    materials.reserve(s_renderer.material_ids.size());

    for (AssetID id : s_renderer.material_ids) {
        Material &mat = s_asset_pack->materials.at(id);
        MaterialData &mat_data = materials.emplace_back();
        mat_data.color = mat.color;
        mat_data.tiling_factor = mat.tiling_factor;
        mat_data.texture_offset = mat.texture_offset;
        mat_data.roughness = mat.roughness;
        mat_data.metallic = mat.metallic;
        mat_data.ao = mat.ao;

        AssetID *material_tex_ids[] = {
            &mat.albedo_texture_id, &mat.normal_texture_id,
            &mat.roughness_texture_id, &mat.metallic_texture_id,
            &mat.ao_texture_id};
        int32_t *buffer_tex_ids[] = {&mat_data.albedo_idx, &mat_data.normal_idx,
                                     &mat_data.roughness_idx,
                                     &mat_data.metallic_idx, &mat_data.ao_idx};
        constexpr int32_t IDS_SIZE =
            sizeof(material_tex_ids) / sizeof(material_tex_ids[0]);

        std::vector<AssetID> &texture_ids = s_renderer.texture_ids;
        for (int32_t i = 0; i < IDS_SIZE; i++) {
            AssetID *material_tex_id = material_tex_ids[i];
            int32_t *buffer_tex_id = buffer_tex_ids[i];

            for (int32_t i = 0; i < texture_ids.size(); i++) {
                if (texture_ids[i] == *material_tex_id) {
                    *buffer_tex_id = i;
                    break;
                }
            }

            if (*buffer_tex_id == -1) {
                *buffer_tex_id = texture_ids.size();
                texture_ids.push_back(*material_tex_id);
            }
        }
    }

    count = materials.size();
    s_renderer.material_uni_buffer.set_data(materials.data(),
                                            count * sizeof(MaterialData));

    for (int32_t i = 0; i < s_renderer.texture_ids.size(); i++) {
        AssetID tex_id = s_renderer.texture_ids[i];
        Texture &tex = s_asset_pack->textures.at(tex_id);

        tex.bind(i);
    }

    /* TODO: assign IDS when texture atlas is ready. */
    int32_t slot = s_renderer.texture_ids.size() * 2;
    s_renderer.shadow_fbo.bind_color_attachment(0, slot);
    s_renderer.shadow_fbo.bind_color_attachment(1, slot + 1);

    for (auto &[shader_id, instance_map] : s_renderer.instances) {
        if (instance_map.empty())
            continue;

        Shader &curr_shader = s_asset_pack->shaders.at(shader_id);
        curr_shader.bind();
        curr_shader.try_set_uniform_1i("u_spot_lights_shadowmaps", slot);
        curr_shader.try_set_uniform_1i("u_point_lights_shadowmaps", slot + 1);

        /*  When I make textures into a texture atlas we will make it
         *  better... */
        for (int32_t i = 0; i < s_renderer.texture_ids.size(); i++) {
            curr_shader.set_uniform_1i("u_textures[" + std::to_string(i) + "]",
                                       i);
        }

        for (auto &[mesh_id, instances] : instance_map) {
            if (instances.empty())
                continue;

            Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
            mesh.vao.vbo_instanced.set_data(
                instances.data(), instances.size() * sizeof(MeshInstance));
            draw_elements_instanced(curr_shader, mesh.vao, instances.size());
        }
    }
}

void shadow_pass_begin(AssetPack &asset_pack) {
    s_asset_pack = &asset_pack;

    s_renderer.point_lights.clear();
    s_renderer.dir_lights.clear();
    s_renderer.spot_lights.clear();
    s_renderer.material_ids.clear();
    s_renderer.texture_ids.clear();

    for (auto &[shader_id, instance_map] : s_renderer.instances) {
        for (auto &[mesh_id, instances] : instance_map)
            instances.clear();

        instance_map.clear();
    }

    s_renderer.instances.clear();
}

void shadow_pass_end() {
    s_renderer.point_lights_uni_buffer.bind();
    s_renderer.dir_lights_uni_buffer.bind();
    s_renderer.spot_lights_uni_buffer.bind();

    int32_t count = s_renderer.point_lights.size();
    int32_t offset = MAX_POINT_LIGHTS * sizeof(PointLightData);
    s_renderer.point_lights_uni_buffer.set_data(s_renderer.point_lights.data(),
                                                count * sizeof(PointLightData));
    s_renderer.point_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    count = s_renderer.dir_lights.size();
    offset = MAX_DIR_LIGHTS * sizeof(DirLightData);
    s_renderer.dir_lights_uni_buffer.set_data(s_renderer.dir_lights.data(),
                                                count * sizeof(DirLightData));
    s_renderer.dir_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    count = s_renderer.spot_lights.size();
    offset = MAX_SPOT_LIGHTS * sizeof(SpotLightData);
    s_renderer.spot_lights_uni_buffer.set_data(s_renderer.spot_lights.data(),
                                               count * sizeof(SpotLightData));
    s_renderer.spot_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    assert(s_renderer.instances.size() == 1 &&
           "More than 1 shader_id (shadow one) submitted");

    InstancesMap &instance_map = s_renderer.instances[0];
    for (auto &[mesh_id, instances] : instance_map) {
        if (instances.empty())
            continue;

        Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
        mesh.vao.vbo_instanced.set_data(
            instances.data(), instances.size() * sizeof(MeshInstance));
    }

    s_renderer.shadow_fbo.bind();
    GL_CALL(glDrawBuffer(GL_NONE));
    GL_CALL(glCullFace(GL_FRONT));

    s_renderer.shadow_fbo.draw_to_depth_map(0);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
    s_renderer.spotlight_shadow_shader.bind();
    for (auto &[mesh_id, instances] : instance_map) {
        if (instances.empty())
            continue;

        Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
        draw_elements_instanced(s_renderer.spotlight_shadow_shader, mesh.vao,
                                instances.size());
    }

    s_renderer.shadow_fbo.draw_to_depth_map(1);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
    s_renderer.pointlight_shadow_shader.bind();
    for (auto &[mesh_id, instances] : instance_map) {
        if (instances.empty())
            continue;

        Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
        draw_elements_instanced(s_renderer.pointlight_shadow_shader, mesh.vao,
                                instances.size());
    }

    GL_CALL(glCullFace(GL_BACK));
}

void submit_mesh(const glm::mat4 &transform, AssetID mesh_id,
                 AssetID material_id, int32_t ent_id, float color_sens) {
    Material &mat = s_asset_pack->materials.at(material_id);
    InstancesMap &group = s_renderer.instances[mat.shader_id];
    std::vector<MeshInstance> &instances = group[mesh_id];

    MeshInstance &instance = instances.emplace_back();
    instance.transform = transform;
    instance.entity_id = ent_id;
    instance.color_sens = color_sens;

    std::vector<AssetID> &material_ids = s_renderer.material_ids;

    auto it =
        std::lower_bound(material_ids.begin(), material_ids.end(), material_id);

    if (it != material_ids.end() && (*it) == material_id) {
        instance.material_idx = (float)std::distance(material_ids.begin(), it);
        return;
    }

    it = material_ids.insert(it, material_id);
    instance.material_idx = std::distance(material_ids.begin(), it);
}

void submit_shadow_pass_mesh(const glm::mat4 &transform, AssetID mesh_id) {
    /* Only one shader in use at a time in shadow passes. */
    InstancesMap &group = s_renderer.instances[0];
    std::vector<MeshInstance> &instances = group[mesh_id];

    MeshInstance &instance = instances.emplace_back();
    instance.transform = transform;
}

static float max_component(const glm::vec3 &v) {
    return glm::max(v.x, glm::max(v.y, v.z));
}

static float light_radius(float constant, float linear, float quadratic,
                          float max_brightness) {
    float num =
        -linear + glm::sqrt(linear * linear -
                            4.0f * quadratic *
                                (constant - (256.0f / 5.0f) * max_brightness));
    return num / (2.0f * quadratic);
}

void submit_point_light(const glm::vec3 &position, const PointLight &light) {
    float radius = light_radius(1.0f, light.linear, light.quadratic,
                                max_component(light.color));
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, radius);

    PointLightData &light_data = s_renderer.point_lights.emplace_back();
    light_data.light_space_matrices = {
        proj * glm::lookAt(position, position + glm::vec3(1.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, -1.0f, 0.0f)),
        proj * glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, -1.0f, 0.0f)),
        proj * glm::lookAt(position, position + glm::vec3(0.0f, 1.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f)),
        proj * glm::lookAt(position, position + glm::vec3(0.0f, -1.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, -1.0f)),
        proj * glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, 1.0f),
                           glm::vec3(0.0f, -1.0f, 0.0f)),
        proj * glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, -1.0f),
                           glm::vec3(0.0f, -1.0f, 0.0f))
    };
    light_data.position_and_linear =
        glm::vec4(position, light.linear);
    light_data.color_and_quadratic =
        glm::vec4(light.color * light.intensity, light.quadratic);
}

void submit_dir_light(const glm::vec3 &rotation, const DirLight &light) {
    DirLightData &light_data = s_renderer.dir_lights.emplace_back();
    light_data.direction = glm::vec4(
        glm::toMat3(glm::quat(rotation)) * glm::vec3(0.0f, 0.0f, -1.0f), 1.0f);
    light_data.color = glm::vec4(light.color, 1.0f);
}

void submit_spot_light(const Transform &transform, const SpotLight &light) {
    float radius = light_radius(1.0f, light.linear, light.quadratic,
                                max_component(light.color));
    glm::vec3 dir = glm::toMat3(glm::quat(transform.rotation)) *
                    glm::vec3(0.0f, 0.0f, -1.0f);

    glm::mat4 proj =
        glm::perspective(glm::radians(2.0f * light.cutoff), 1.0f, 0.1f, radius);
    glm::mat4 view = glm::lookAt(transform.position, transform.position + dir,
                                 glm::vec3(0.0f, 1.0f, 0.0f));

    SpotLightData &light_data = s_renderer.spot_lights.emplace_back();
    light_data.light_space_mat = proj * view;
    light_data.pos_and_cutoff =
        glm::vec4(transform.position, glm::cos(glm::radians(light.cutoff)));
    light_data.dir_and_outer_cutoff = glm::vec4(
        dir, glm::cos(glm::radians(light.cutoff - light.edge_smoothness)));
    light_data.color_and_linear =
        glm::vec4(light.color * light.intensity, light.linear);
    light_data.quadratic = light.quadratic;
}

void draw_to_screen_quad() {
    GL_CALL(glDisable(GL_DEPTH_TEST));
    draw_arrays(s_renderer.screen_quad_shader, s_renderer.screen_quad_vao, 6);
    GL_CALL(glEnable(GL_DEPTH_TEST));
}

void draw_arrays(const Shader &shader, const VertexArray &vao,
                 uint32_t vertices_count) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, vertices_count));
}

void draw_arrays_instanced(const Shader &shader, const VertexArray &vao,
                           uint32_t vertices_count, uint32_t instances_count) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawArraysInstanced(GL_TRIANGLES, 0, vertices_count,
                                  instances_count));
}

void draw_elements(const Shader &shader, const VertexArray &vao) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawElements(GL_TRIANGLES, vao.ibo.indices_count, GL_UNSIGNED_INT,
                           nullptr));
}

void draw_elements_instanced(const Shader &shader, const VertexArray &vao,
                             uint32_t instances_count) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawElementsInstanced(GL_TRIANGLES, vao.ibo.indices_count,
                                    GL_UNSIGNED_INT, nullptr, instances_count));
}

} // namespace eng::Renderer
