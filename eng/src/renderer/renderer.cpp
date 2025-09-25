#include "eng/renderer/renderer.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include "GLFW/glfw3.h"
#include "glm/fwd.hpp"
#include "glm/gtc/constants.hpp"

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace eng::renderer {

struct GPU {
    char *vendor;
    char *device_name;
    char *opengl_version;
    char *glsl_version;

    int32_t texture_units = 0;
    int32_t max_3D_texture_size = 0;
    int32_t max_array_texture_layers = 0;
    int32_t max_geom_invocations = 0;
};

using InstancesMap = std::unordered_map<AssetID, std::vector<MeshInstance>>;
using ShaderGroupedInstances = std::unordered_map<AssetID, InstancesMap>;

struct Renderer {
    GPU gpu;
    TextureSlots slots;

    VertexArray screen_quad_vao;
    Shader screen_quad_shader;

    UniformBuffer camera_uni_buffer;

    UniformBuffer dir_lights_uni_buffer;
    std::vector<DirLightData> dir_lights;

    UniformBuffer point_lights_uni_buffer;
    std::vector<PointLightData> point_lights;

    UniformBuffer spot_lights_uni_buffer;
    std::vector<SpotLightData> spot_lights;

    Framebuffer shadow_fbo;
    Shader dirlight_shadow_shader;
    Shader pointlight_shadow_shader;
    Shader spotlight_shadow_shader;

    GLuint random_offset_tex_id = 0;
    UniformBuffer soft_shadow_uni_buffer;
    SoftShadowProps soft_shadow_props;
    SoftShadowProps cached_soft_shadow_props;

    UniformBuffer material_uni_buffer;
    std::vector<AssetID> material_ids;

    UniformBuffer tex_records_uni_buffer;
    std::vector<AssetID> tex_record_ids;

    ShaderGroupedInstances instances;
};

static Renderer s_renderer{};
static AssetPack *s_asset_pack{};
static const CameraData *s_active_camera{};

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

static void soft_shadow_random_offset_texture_create() {
    std::default_random_engine eng{};
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

    int32_t window_size = s_renderer.cached_soft_shadow_props.offsets_tex_size;
    int32_t filter_size =
        s_renderer.cached_soft_shadow_props.offsets_filter_size;
    int32_t buffer_size =
        window_size * window_size * filter_size * filter_size * 2;

    std::vector<float> tex_data;
    tex_data.resize(buffer_size);

    int32_t idx = 0;
    for (int32_t y = 0; y < window_size; y++) {
        for (int32_t x = 0; x < window_size; x++) {
            for (int32_t v = filter_size - 1; v >= 0; v--) {
                for (int32_t u = 0; u < filter_size; u++) {
                    assert(idx < tex_data.size());

                    float x =
                        ((float)u + 0.5f + dist(eng)) / (float)filter_size;
                    float y =
                        ((float)v + 0.5f + dist(eng)) / (float)filter_size;

                    tex_data[idx + 0] =
                        glm::sqrt(y) * glm::cos(glm::two_pi<float>() * x);
                    tex_data[idx + 1] =
                        glm::sqrt(y) * glm::sin(glm::two_pi<float>() * x);

                    idx += 2;
                }
            }
        }
    }

    if (s_renderer.random_offset_tex_id != 0)
        GL_CALL(glDeleteTextures(1, &s_renderer.random_offset_tex_id));

    int32_t filter_samples = filter_size * filter_size;
    GL_CALL(glGenTextures(1, &s_renderer.random_offset_tex_id));
    GL_CALL(glBindTexture(GL_TEXTURE_3D, s_renderer.random_offset_tex_id));
    GL_CALL(glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32F, filter_samples / 2,
                           window_size, window_size));
    GL_CALL(glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, filter_samples / 2,
                            window_size, window_size, GL_RGBA, GL_FLOAT,
                            tex_data.data()));
    GL_CALL(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glBindTexture(GL_TEXTURE_3D, 0));
}

bool init() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    GPU &gpu_spec = s_renderer.gpu;
    GL_CALL(gpu_spec.vendor = (char *)glGetString(GL_VENDOR));
    GL_CALL(gpu_spec.device_name = (char *)glGetString(GL_RENDERER));
    GL_CALL(gpu_spec.opengl_version = (char *)glGetString(GL_VERSION));
    GL_CALL(gpu_spec.glsl_version =
                (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));

    GL_CALL(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &gpu_spec.texture_units));
    GL_CALL(glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,
                          &gpu_spec.max_array_texture_layers));
    GL_CALL(
        glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &gpu_spec.max_3D_texture_size));
    GL_CALL(glGetIntegerv(GL_MAX_GEOMETRY_SHADER_INVOCATIONS,
                          &gpu_spec.max_geom_invocations));

    printf("GPU Vendor: %s\n", gpu_spec.vendor);
    printf("GPU Device: %s\n", gpu_spec.device_name);
    printf("OpenGL version: %s\n", gpu_spec.opengl_version);
    printf("GLSL version: %s\n", gpu_spec.glsl_version);
    printf("Max terxture image units: %d\n", gpu_spec.texture_units);
    printf("Max array texture layers: %d\n", gpu_spec.max_array_texture_layers);
    printf("Max 3D texture size: %d\n", gpu_spec.max_3D_texture_size);
    printf("Max geometry shader invocations: %d\n",
           gpu_spec.max_geom_invocations);

    s_renderer.slots.rgba_atlas = 0;
    s_renderer.slots.rgb_atlas = 1;
    s_renderer.slots.r_atlas = 2;
    s_renderer.slots.dir_csm_shadowmaps = gpu_spec.texture_units - 1;
    s_renderer.slots.point_lights_shadowmaps = gpu_spec.texture_units - 2;
    s_renderer.slots.spot_lights_shadowmaps = gpu_spec.texture_units - 3;
    s_renderer.slots.random_offsets_texture = gpu_spec.texture_units - 4;

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

    size = MAX_DIR_LIGHTS * sizeof(DirLightData) + sizeof(int32_t);
    s_renderer.dir_lights_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.dir_lights_uni_buffer.bind_buffer_range(DIR_LIGHTS_BINDING, 0,
                                                       size);

    size = MAX_POINT_LIGHTS * sizeof(PointLightData) + sizeof(int32_t);
    s_renderer.point_lights_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.point_lights_uni_buffer.bind_buffer_range(POINT_LIGHTS_BINDING,
                                                         0, size);

    size = MAX_SPOT_LIGHTS * sizeof(SpotLightData) + sizeof(int32_t);
    s_renderer.spot_lights_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.spot_lights_uni_buffer.bind_buffer_range(SPOT_LIGHTS_BINDING, 0,
                                                        size);

    size = sizeof(SoftShadowProps);
    s_renderer.soft_shadow_uni_buffer= UniformBuffer::create(nullptr, size);
    s_renderer.soft_shadow_uni_buffer.bind_buffer_range(
        SOFT_SHADOW_PROPS_BINDING, 0, size);

    {
        ColorAttachmentSpec spec;
        spec.type = ColorAttachmentType::TEX_2D_ARRAY_SHADOW;
        spec.format = TextureFormat::DEPTH_32F;
        spec.wrap = GL_CLAMP_TO_BORDER;
        spec.min_filter = spec.mag_filter = GL_NEAREST;
        spec.size = {2048, 2048};
        spec.layers = MAX_DIR_LIGHTS * CASCADES_COUNT;
        spec.gen_minmaps = false;
        spec.border_color = glm::vec4(1.0f);

        s_renderer.shadow_fbo = Framebuffer::create();
        s_renderer.shadow_fbo.bind();
        s_renderer.shadow_fbo.add_color_attachment(spec);

        spec.size = {512, 512};
        spec.layers = MAX_DIR_LIGHTS * 6;
        s_renderer.shadow_fbo.add_color_attachment(spec);

        spec.layers = MAX_SPOT_LIGHTS;
        s_renderer.shadow_fbo.add_color_attachment(spec);
    }

    {
        s_renderer.dirlight_shadow_shader = Shader::create();

        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/depth.vert";
        spec.fragment_shader.path = "resources/shaders/depth.frag";
        spec.geometry_shader = {
            .path = "resources/shaders/shadows/dirlight.geom",
            .replacements = {
                {
                    "${DIR_LIGHTS_BINDING}",
                    std::to_string(renderer::DIR_LIGHTS_BINDING)
                },
                {
                    "${MAX_DIR_LIGHTS}",
                    std::to_string(renderer::MAX_DIR_LIGHTS)
                },
                {
                    "${INVOCATIONS}",
                    std::to_string(s_renderer.gpu.max_geom_invocations)
                },
                {
                    "${CASCADES_COUNT}",
                    std::to_string(renderer::CASCADES_COUNT)
                },
                {
                    "${MAX_VERTICES}",
                    std::to_string(renderer::CASCADES_COUNT * 3)
                }
            }
        };

        assert(s_renderer.dirlight_shadow_shader.build(spec) &&
               "Dirlight shadow shader not built");
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


    soft_shadow_random_offset_texture_create();

    size = MAX_MATERIALS * sizeof(MaterialData);
    s_renderer.material_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.material_uni_buffer.bind_buffer_range(MATERIALS_BINDING, 0,
                                                     size);

    size = MAX_MATERIALS * sizeof(TexRecordData) * 5;
    s_renderer.tex_records_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.tex_records_uni_buffer.bind_buffer_range(TEX_RECORDS_BINDING, 0,
                                                        size);

    return true;
}

void shutdown() {
    s_renderer.camera_uni_buffer.destroy();
    s_renderer.dir_lights_uni_buffer.destroy();
    s_renderer.point_lights_uni_buffer.destroy();
    s_renderer.spot_lights_uni_buffer.destroy();
    s_renderer.soft_shadow_uni_buffer.destroy();
    s_renderer.material_uni_buffer.destroy();
    s_renderer.tex_records_uni_buffer.destroy();

    s_renderer.shadow_fbo.destroy();
    s_renderer.dirlight_shadow_shader.destroy();
    s_renderer.spotlight_shadow_shader.destroy();
    s_renderer.pointlight_shadow_shader.destroy();
}

void scene_begin(const CameraData &camera, AssetPack &asset_pack) {
    s_asset_pack = &asset_pack;
    s_active_camera = &camera;

    assert(s_asset_pack && "Empty asset pack object");

    s_renderer.dir_lights.clear();
    s_renderer.point_lights.clear();
    s_renderer.spot_lights.clear();
    s_renderer.material_ids.clear();
    s_renderer.tex_record_ids.clear();

    for (auto &[shader_id, instance_map] : s_renderer.instances) {
        for (auto &[mesh_id, instances] : instance_map)
            instances.clear();

        instance_map.clear();
    }

    s_renderer.instances.clear();

    s_renderer.camera_uni_buffer.bind();
    s_renderer.camera_uni_buffer.set_data(&camera, sizeof(CameraData));

    /* Reconstruct if values changed. */
    if (memcmp(&s_renderer.soft_shadow_props,
               &s_renderer.cached_soft_shadow_props,
               sizeof(SoftShadowProps)) != 0) {
        s_renderer.cached_soft_shadow_props = s_renderer.soft_shadow_props;
        soft_shadow_random_offset_texture_create();
    }

    s_renderer.soft_shadow_uni_buffer.bind();
    s_renderer.soft_shadow_uni_buffer.set_data(
        &s_renderer.cached_soft_shadow_props, sizeof(SoftShadowProps));
}

void scene_end() {
    s_renderer.dir_lights_uni_buffer.bind();
    s_renderer.point_lights_uni_buffer.bind();
    s_renderer.spot_lights_uni_buffer.bind();

    int32_t count = s_renderer.dir_lights.size();
    int32_t offset = MAX_DIR_LIGHTS * sizeof(DirLightData);
    s_renderer.dir_lights_uni_buffer.set_data(s_renderer.dir_lights.data(),
                                                count * sizeof(DirLightData));
    s_renderer.dir_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    count = s_renderer.point_lights.size();
    offset = MAX_POINT_LIGHTS * sizeof(PointLightData);
    s_renderer.point_lights_uni_buffer.set_data(s_renderer.point_lights.data(),
                                                count * sizeof(PointLightData));
    s_renderer.point_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    count = s_renderer.spot_lights.size();
    offset = MAX_SPOT_LIGHTS * sizeof(SpotLightData);
    s_renderer.spot_lights_uni_buffer.set_data(s_renderer.spot_lights.data(),
                                               count * sizeof(SpotLightData));
    s_renderer.spot_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    std::vector<MaterialData> materials;
    materials.reserve(s_renderer.material_ids.size());

    std::vector<TexRecordData> tex_records_data;
    tex_records_data.reserve(s_renderer.material_ids.size());

    std::vector<AssetID> &tex_record_ids = s_renderer.tex_record_ids;

    for (AssetID id : s_renderer.material_ids) {
        Material &mat = s_asset_pack->materials.at(id);
        MaterialData &mat_data = materials.emplace_back();
        mat_data.color = mat.color;
        mat_data.tiling_factor = mat.tiling_factor;
        mat_data.texture_offset = mat.texture_offset;
        mat_data.roughness = mat.roughness;
        mat_data.metallic = mat.metallic;
        mat_data.ao = mat.ao;

        std::array<AssetID, 5> tex_records = {
            mat.albedo_tex_record_id, mat.normal_tex_record_id,
            mat.roughness_tex_record_id, mat.metallic_tex_record_id,
            mat.ao_tex_record_id};
        std::array<int32_t *, 5> record_idxs = {
            &mat_data.albedo_record_idx, &mat_data.normal_record_idx,
            &mat_data.roughness_record_idx, &mat_data.metallic_record_idx,
            &mat_data.ao_record_idx};

        for (int32_t i = 0; i < tex_records.size(); i++) {
            int32_t target_idx = -1;
            for (int32_t j = 0; j < tex_record_ids.size(); j++) {
                if (tex_record_ids[j] == tex_records[i]) {
                    target_idx = j;
                    break;
                }
            }

            if (target_idx == -1) {
                tex_record_ids.push_back(tex_records[i]);
                target_idx = tex_record_ids.size() - 1;

                TextureRecord &tr =
                    s_asset_pack->tex_records.at(tex_records[i]);
                AtlasContext &ac = s_asset_pack->atlases.at(tr.owning_atlas);

                TexRecordData &rec = tex_records_data.emplace_back();
                rec.offset = ac.atlas.to_uv(tr.offset);
                rec.size = ac.atlas.to_uv(tr.size);
                rec.layer = tr.layer;
                rec.record_id = tex_records[i];
            }

            *record_idxs[i] = target_idx;
        }
    }

    count = materials.size();
    s_renderer.material_uni_buffer.set_data(materials.data(),
                                            count * sizeof(MaterialData));

    count = tex_records_data.size();
    s_renderer.tex_records_uni_buffer.set_data(tex_records_data.data(),
                                               count * sizeof(TexRecordData));

    s_renderer.shadow_fbo.bind_color_attachment(
        0, s_renderer.slots.dir_csm_shadowmaps);
    s_renderer.shadow_fbo.bind_color_attachment(
        1, s_renderer.slots.point_lights_shadowmaps);
    s_renderer.shadow_fbo.bind_color_attachment(
        2, s_renderer.slots.spot_lights_shadowmaps);

    GL_CALL(
        glActiveTexture(GL_TEXTURE0 + s_renderer.slots.random_offsets_texture));
    GL_CALL(glBindTexture(GL_TEXTURE_3D, s_renderer.random_offset_tex_id));

    s_asset_pack->atlases.at(AssetPack::ATLAS_RGBA8)
        .atlas.bind(s_renderer.slots.rgba_atlas);
    s_asset_pack->atlases.at(AssetPack::ATLAS_RGB8)
        .atlas.bind(s_renderer.slots.rgb_atlas);
    s_asset_pack->atlases.at(AssetPack::ATLAS_R8)
        .atlas.bind(s_renderer.slots.r_atlas);

    std::array<float, CASCADES_COUNT> cascade_distances = {
        s_active_camera->far_clip / 50.0f,
        s_active_camera->far_clip / 25.0f,
        s_active_camera->far_clip / 10.0f,
        s_active_camera->far_clip / 2.0f,
        s_active_camera->far_clip
    };

    for (auto &[shader_id, instance_map] : s_renderer.instances) {
        if (instance_map.empty())
            continue;

        Shader &curr_shader = s_asset_pack->shaders.at(shader_id);
        curr_shader.bind();

        for (int32_t i = 0; i < CASCADES_COUNT; i++)
            curr_shader.try_set_uniform_1f("u_cascade_distances[" +
                                               std::to_string(i) + "]",
                                           cascade_distances[i]);

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

void shadow_pass_begin(const CameraData &camera, AssetPack &asset_pack) {
    s_asset_pack = &asset_pack;
    s_active_camera = &camera;

    s_renderer.dir_lights.clear();
    s_renderer.point_lights.clear();
    s_renderer.spot_lights.clear();
    s_renderer.material_ids.clear();
    s_renderer.tex_record_ids.clear();

    for (auto &[shader_id, instance_map] : s_renderer.instances) {
        for (auto &[mesh_id, instances] : instance_map)
            instances.clear();

        instance_map.clear();
    }

    s_renderer.instances.clear();
}

void shadow_pass_end() {
    if (s_renderer.instances.empty())
        return;

    s_renderer.dir_lights_uni_buffer.bind();
    s_renderer.point_lights_uni_buffer.bind();
    s_renderer.spot_lights_uni_buffer.bind();

    int32_t count = s_renderer.dir_lights.size();
    int32_t offset = MAX_DIR_LIGHTS * sizeof(DirLightData);
    s_renderer.dir_lights_uni_buffer.set_data(s_renderer.dir_lights.data(),
                                                count * sizeof(DirLightData));
    s_renderer.dir_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    count = s_renderer.point_lights.size();
    offset = MAX_POINT_LIGHTS * sizeof(PointLightData);
    s_renderer.point_lights_uni_buffer.set_data(s_renderer.point_lights.data(),
                                                count * sizeof(PointLightData));
    s_renderer.point_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

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
    s_renderer.dirlight_shadow_shader.bind();
    for (auto &[mesh_id, instances] : instance_map) {
        if (instances.empty())
            continue;

        Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
        draw_elements_instanced(s_renderer.dirlight_shadow_shader, mesh.vao,
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

    s_renderer.shadow_fbo.draw_to_depth_map(2);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
    s_renderer.spotlight_shadow_shader.bind();
    for (auto &[mesh_id, instances] : instance_map) {
        if (instances.empty())
            continue;

        Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
        draw_elements_instanced(s_renderer.spotlight_shadow_shader, mesh.vao,
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

static std::vector<glm::vec4>
frustrum_corners_world_space(const glm::mat4 &proj_view) {
    glm::mat4 inv = glm::inverse(proj_view);
    std::vector<glm::vec4> corners;

    for (int32_t x = 0; x < 2; x++) {
        for (int32_t y = 0; y < 2; y++) {
            for (int32_t z = 0; z < 2; z++) {
                glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f,
                                               2.0f * z - 1.0f, 1.0f);
                corners.push_back(pt / pt.w);
            }
        }
    }

    return corners;
}

void submit_dir_light(const glm::vec3 &rotation, const DirLight &light) {
    if (s_renderer.dir_lights.size() >= MAX_DIR_LIGHTS)
        return;

    std::array<float, CASCADES_COUNT> near_planes = {
        s_active_camera->near_clip,
        s_active_camera->far_clip / 50.0f,
        s_active_camera->far_clip / 25.0f,
        s_active_camera->far_clip / 10.0f,
        s_active_camera->far_clip / 2.0f
    };
    std::array<float, CASCADES_COUNT> far_planes = {
        near_planes[1],
        near_planes[2],
        near_planes[3],
        near_planes[4],
        s_active_camera->far_clip
    };

    std::array<glm::mat4, CASCADES_COUNT> cascaded_mats;
    glm::vec3 dir =
        glm::toMat3(glm::quat(rotation)) * glm::vec3(0.0f, 0.0f, -1.0f);

    for (int32_t i = 0; i < near_planes.size(); i++) {
        glm::mat4 proj = glm::perspective(glm::radians(s_active_camera->fov),
                                          s_active_camera->viewport.x /
                                              s_active_camera->viewport.y,
                                          near_planes[i], far_planes[i]);
        std::vector<glm::vec4> view_corners =
            frustrum_corners_world_space(proj * s_active_camera->view);

        glm::vec3 center(0.0f);
        for (const glm::vec4 &corner : view_corners)
             center += glm::vec3(corner);

        center /= view_corners.size();

        glm::mat4 light_view =
            glm::lookAt(center + dir, center, glm::vec3(0.0f, 1.0f, 0.0f));
        float xmin =  FLT_MAX;
        float xmax = -FLT_MAX;
        float ymin =  FLT_MAX;
        float ymax = -FLT_MAX;
        float zmin =  FLT_MAX;
        float zmax = -FLT_MAX;
        for (const glm::vec4 &corner : view_corners) {
            glm::vec4 trf = light_view * corner;
            xmin = glm::min(xmin, trf.x);
            xmax = glm::max(xmax, trf.x);
            ymin = glm::min(ymin, trf.y);
            ymax = glm::max(ymax, trf.y);
            zmin = glm::min(zmin, trf.z);
            zmax = glm::max(zmax, trf.z);
        }

        constexpr float Z_MULT = 10.0f;
        zmin = (zmin < 0.0f ? zmin * Z_MULT : zmin / Z_MULT);
        zmax = (zmax < 0.0f ? zmax / Z_MULT : zmax * Z_MULT);

        glm::mat4 light_proj = glm::ortho(xmin, xmax, ymin, ymax, zmin, zmax);
        cascaded_mats[i] = light_proj * light_view;
    }

    DirLightData &light_data = s_renderer.dir_lights.emplace_back();
    light_data.cascade_mats = cascaded_mats;
    light_data.direction = glm::vec4(
        glm::toMat3(glm::quat(rotation)) * glm::vec3(0.0f, 0.0f, -1.0f), 1.0f);
    light_data.color = glm::vec4(light.color, 1.0f);
}

void submit_point_light(const glm::vec3 &position, const PointLight &light) {
    if (s_renderer.point_lights.size() >= MAX_POINT_LIGHTS)
        return;

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

void submit_spot_light(const Transform &transform, const SpotLight &light) {
    if (s_renderer.spot_lights.size() >= MAX_SPOT_LIGHTS)
        return;

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

SoftShadowProps &soft_shadow_props() {
    return s_renderer.soft_shadow_props;
}

TextureSlots texture_slots() {
    return s_renderer.slots;
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
