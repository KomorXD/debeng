#include "eng/renderer/renderer.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/renderer/primitives.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include "GLFW/glfw3.h"
#include "eng/timer.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/constants.hpp"

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

using MeshGroup = std::unordered_map<AssetID, std::vector<MeshInstance>>;
using MaterialGroup = std::unordered_map<AssetID, MeshGroup>;
using ShaderGroup = std::unordered_map<AssetID, MaterialGroup>;

struct Renderer {
    GPU gpu;
    TextureSlots slots;
    RenderStats stats;

    Shader post_proc_combine_shader;

    VertexArray cubemap_vao;
    Shader cubemap_shader;

    Shader equirec_to_cubemap_shader;
    Shader cubemap_convolution_shader;
    Shader cubemap_prefilter_shader;

    Texture brdf_map;

    Texture bloom_texture;
    Shader bloom_filter;
    Shader bloom_downsampler;
    Shader bloom_upsampler;

    UniformBuffer camera_uni_buffer;

    ShaderStorage dir_lights_storage;
    std::vector<DirLightData> dir_lights;

    ShaderStorage point_lights_storage;
    std::vector<PointLightData> point_lights;

    ShaderStorage spot_lights_storage;
    std::vector<SpotLightData> spot_lights;

    Framebuffer shadow_fbo;
    Shader dirlight_shadow_shader;
    Shader pointlight_shadow_shader;
    Shader spotlight_shadow_shader;

    GLuint random_offset_tex_id = 0;
    UniformBuffer soft_shadow_uni_buffer;
    SoftShadowProps soft_shadow_props;
    SoftShadowProps cached_soft_shadow_props;

    UniformBuffer draw_params_uni_buffer;
    std::vector<DrawParams> draw_params;

    ShaderGroup shader_render_group;
};

static Renderer s_renderer{};
static AssetPack *s_asset_pack{};
static const CameraData *s_active_camera{};
static EnvMap *s_envmap{};

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

    s_renderer.slots.albedo = 0;
    s_renderer.slots.normal = 1;
    s_renderer.slots.roughness = 2;
    s_renderer.slots.metallic = 3;
    s_renderer.slots.ao = 4;
    s_renderer.slots.irradiance_map = 5;
    s_renderer.slots.prefilter_map = 6;
    s_renderer.slots.brdf_lut = 7;
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

    s_renderer.post_proc_combine_shader = Shader::create();
    assert(s_renderer.post_proc_combine_shader.build_compute(
               {.path = "resources/shaders/post_proc/post_process_combine.comp",
                .replacements = {{"${CAMERA_BINDING}",
                                  std::to_string(CAMERA_BINDING)}}}) &&
           "Failed to build post-process combine shader");

    s_renderer.post_proc_combine_shader.bind();

    {
        std::vector<float> vertices = skybox_vertex_data();
        s_renderer.cubemap_vao = VertexArray::create();

        VertexBuffer vbo = VertexBuffer::create();
        vbo.allocate(vertices.data(), vertices.size() * sizeof(float));

        VertexBufferLayout layout;
        layout.push_float(3); // 0 - position

        s_renderer.cubemap_vao.add_vertex_buffer(vbo, layout);

        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/skybox.vert";
        spec.vertex_shader.replacements.push_back(
            {.pattern = "${CAMERA_BINDING}",
             .target = std::to_string(CAMERA_BINDING)});
        spec.fragment_shader.path = "resources/shaders/skybox.frag";

        s_renderer.cubemap_shader = Shader::create();
        assert(s_renderer.cubemap_shader.build(spec) &&
               "Failed to build skybox shader");

        s_renderer.cubemap_shader.bind();
        s_renderer.cubemap_shader.set_uniform_1i("u_cubemap", 0);
    }

    s_renderer.equirec_to_cubemap_shader = Shader::create();
    assert(s_renderer.equirec_to_cubemap_shader.build_compute(
               {.path = "resources/shaders/envmap/equirec_to_cubemap.comp"}) &&
           "Failed to build equirec to cubemap shader");

    s_renderer.cubemap_convolution_shader = Shader::create();
    assert(s_renderer.cubemap_convolution_shader.build_compute(
               {.path = "resources/shaders/envmap/cubemap_convolution.comp"}) &&
           "Failed to build cubemap convolution shader");

    s_renderer.cubemap_prefilter_shader = Shader::create();
    assert(
        s_renderer.cubemap_prefilter_shader.build_compute(
            {.path = "resources/shaders/envmap/prefilter_convolution.comp"}) &&
        "Failed to build cubemap prefilter shader");

    {
        TextureSpec spec;
        spec.format = TextureFormat::RG16F;
        spec.size = {512, 512};
        spec.min_filter = spec.mag_filter = GL_LINEAR;
        spec.wrap = GL_CLAMP_TO_EDGE;
        s_renderer.brdf_map = Texture::create(nullptr, spec);
    }

    s_renderer.slots.prefilter_mips = 5;

    {
        Shader brdf_shader = Shader::create();
        assert(brdf_shader.build_compute(
                   {.path = "resources/shaders/envmap/brdf.comp"}) &&
               "Failed to build BRDF shader");

        glm::ivec3 groups;
        groups.x = (s_renderer.brdf_map.spec.size.x + 15) / 16;
        groups.y = (s_renderer.brdf_map.spec.size.y + 15) / 16;
        groups.z = 1;

        s_renderer.brdf_map.bind_image(0, 0, ImageAccess::WRITE);
        brdf_shader.dispatch_compute(groups);
        brdf_shader.destroy();

        s_renderer.brdf_map.bind();
        GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
    }

    uint32_t size = sizeof(CameraData);
    s_renderer.camera_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.camera_uni_buffer.bind_buffer_range(CAMERA_BINDING, 0, size);

    size = MAX_DIR_LIGHTS * sizeof(DirLightData) + sizeof(int32_t) * 4;
    s_renderer.dir_lights_storage = ShaderStorage::create(nullptr, size);
    s_renderer.dir_lights_storage.bind_buffer_range(DIR_LIGHTS_BINDING, 0,
                                                       size);

    size = MAX_POINT_LIGHTS * sizeof(PointLightData) + sizeof(int32_t) * 4;
    s_renderer.point_lights_storage = ShaderStorage::create(nullptr, size);
    s_renderer.point_lights_storage.bind_buffer_range(POINT_LIGHTS_BINDING,
                                                         0, size);

    size = MAX_SPOT_LIGHTS * sizeof(SpotLightData) + sizeof(int32_t) * 4;
    s_renderer.spot_lights_storage = ShaderStorage::create(nullptr, size);
    s_renderer.spot_lights_storage.bind_buffer_range(SPOT_LIGHTS_BINDING, 0,
                                                        size);

    size = sizeof(SoftShadowProps);
    s_renderer.soft_shadow_uni_buffer= UniformBuffer::create(nullptr, size);
    s_renderer.soft_shadow_uni_buffer.bind_buffer_range(
        SOFT_SHADOW_PROPS_BINDING, 0, size);

    s_renderer.bloom_filter = Shader::create();
    assert(s_renderer.bloom_filter.build_compute(
               {"resources/shaders/bloom/filter.comp",
                {{"${CAMERA_BINDING}", std::to_string(CAMERA_BINDING)}}}) &&
           "Failed to build bloom filter shader");

    s_renderer.bloom_downsampler = Shader::create();
    assert(s_renderer.bloom_downsampler.build_compute(
               {"resources/shaders/bloom/downsampler.comp", {}}) &&
           "Failed to build bloom downsampler shader");

    s_renderer.bloom_upsampler = Shader::create();
    assert(s_renderer.bloom_upsampler.build_compute(
               {"resources/shaders/bloom/upsampler.comp",
                {{"${CAMERA_BINDING}", std::to_string(CAMERA_BINDING)}}}) &&
           "Failed to build bloom upsampler shader");

    {
        TextureSpec spec;
        spec.format = TextureFormat::RGBA16F;
        spec.size = {800, 600};
        spec.min_filter = GL_LINEAR_MIPMAP_LINEAR;
        spec.mag_filter = GL_LINEAR;
        spec.wrap = GL_CLAMP_TO_EDGE;
        spec.mips = 5;

        s_renderer.bloom_texture = Texture::create_storage(spec);
    }

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
        spec.layers = MAX_POINT_LIGHTS * 6;
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

    size = MAX_DRAW_PARAMS * sizeof(DrawParams);
    s_renderer.draw_params_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.draw_params_uni_buffer.bind_buffer_range(DRAW_PARAMS_BINDING, 0,
                                                        size);

    return true;
}

void shutdown() {
    s_renderer.camera_uni_buffer.destroy();
    s_renderer.dir_lights_storage.destroy();
    s_renderer.point_lights_storage.destroy();
    s_renderer.spot_lights_storage.destroy();
    s_renderer.soft_shadow_uni_buffer.destroy();
    s_renderer.draw_params_uni_buffer.destroy();

    s_renderer.shadow_fbo.destroy();
    s_renderer.dirlight_shadow_shader.destroy();
    s_renderer.spotlight_shadow_shader.destroy();
    s_renderer.pointlight_shadow_shader.destroy();

    s_renderer.post_proc_combine_shader.destroy();

    s_renderer.cubemap_vao.destroy();
    s_renderer.cubemap_shader.destroy();

    s_renderer.equirec_to_cubemap_shader.destroy();
    s_renderer.cubemap_convolution_shader.destroy();
    s_renderer.cubemap_prefilter_shader.destroy();

    s_renderer.bloom_texture.destroy();
    s_renderer.bloom_filter.destroy();
    s_renderer.bloom_downsampler.destroy();
    s_renderer.bloom_upsampler.destroy();
}

void scene_begin(const CameraData &camera, AssetPack &asset_pack) {
    s_asset_pack = &asset_pack;
    s_active_camera = &camera;

    assert(s_asset_pack && "Empty asset pack object");

    s_renderer.dir_lights.clear();
    s_renderer.point_lights.clear();
    s_renderer.spot_lights.clear();
    s_renderer.draw_params.clear();

    for (auto &[shader_id, shader_group] : s_renderer.shader_render_group) {
        for (auto &[mat_id, mat_group] : shader_group) {
            for (auto &[mesh_id, instances] : mat_group)
                instances.clear();

            mat_group.clear();
        }

        shader_group.clear();
    }

    s_renderer.shader_render_group.clear();

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
    Timer t;
    t.start();

    s_renderer.dir_lights_storage.bind();
    s_renderer.point_lights_storage.bind();
    s_renderer.spot_lights_storage.bind();
    s_renderer.draw_params_uni_buffer.bind();

    int32_t count = s_renderer.dir_lights.size();
    int32_t offset = sizeof(int32_t) * 4;
    s_renderer.dir_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.dir_lights_storage.set_data(
        s_renderer.dir_lights.data(), count * sizeof(DirLightData), offset);

    count = s_renderer.point_lights.size();
    s_renderer.point_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.point_lights_storage.set_data(
        s_renderer.point_lights.data(), count * sizeof(PointLightData), offset);

    count = s_renderer.spot_lights.size();
    s_renderer.spot_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.spot_lights_storage.set_data(
        s_renderer.spot_lights.data(), count * sizeof(SpotLightData), offset);

    count = s_renderer.draw_params.size();
    s_renderer.draw_params_uni_buffer.set_data(s_renderer.draw_params.data(),
                                               count * sizeof(DrawParams));

    s_renderer.shadow_fbo.bind_color_attachment(
        0, s_renderer.slots.dir_csm_shadowmaps);
    s_renderer.shadow_fbo.bind_color_attachment(
        1, s_renderer.slots.point_lights_shadowmaps);
    s_renderer.shadow_fbo.bind_color_attachment(
        2, s_renderer.slots.spot_lights_shadowmaps);

    GL_CALL(
        glActiveTexture(GL_TEXTURE0 + s_renderer.slots.random_offsets_texture));
    GL_CALL(glBindTexture(GL_TEXTURE_3D, s_renderer.random_offset_tex_id));

    std::array<float, CASCADES_COUNT> cascade_distances = {
        s_active_camera->far_clip / 50.0f, s_active_camera->far_clip / 25.0f,
        s_active_camera->far_clip / 10.0f, s_active_camera->far_clip / 2.0f,
        s_active_camera->far_clip};

    s_envmap->irradiance_map.bind(s_renderer.slots.irradiance_map);
    s_envmap->prefilter_map.bind(s_renderer.slots.prefilter_map);
    s_renderer.brdf_map.bind(s_renderer.slots.brdf_lut);

    for (auto &[shader_id, material_group] : s_renderer.shader_render_group) {
        Shader &curr_shader = s_asset_pack->shaders.at(shader_id);
        curr_shader.bind();

        for (int32_t i = 0; i < CASCADES_COUNT; i++)
            curr_shader.try_set_uniform_1f("u_cascade_distances[" +
                                               std::to_string(i) + "]",
                                           cascade_distances[i]);

        for (auto &[mat_id, mesh_group] : material_group) {
            Material &mat = s_asset_pack->materials.at(mat_id);
            curr_shader.try_set_uniform_4f("u_material.color", mat.color);
            curr_shader.try_set_uniform_2f("u_material.tiling_factor",
                                           mat.tiling_factor);
            curr_shader.try_set_uniform_2f("u_material.texture_offset",
                                           mat.texture_offset);
            curr_shader.try_set_uniform_1f("u_material.roughness",
                                           mat.roughness);
            curr_shader.try_set_uniform_1f("u_material.metallic", mat.metallic);
            curr_shader.try_set_uniform_1f("u_material.ao", mat.ao);

            std::array<AssetID, 5> tex_ids = {
                mat.albedo_texture_id, mat.normal_texture_id,
                mat.roughness_texture_id, mat.metallic_texture_id,
                mat.ao_texture_id};
            std::array<int32_t, 5> tex_bindings = {
                s_renderer.slots.albedo, s_renderer.slots.normal,
                s_renderer.slots.roughness, s_renderer.slots.metallic,
                s_renderer.slots.ao};
            for (int32_t i = 0; i < tex_ids.size(); i++) {
                Texture &tex = s_asset_pack->textures.at(tex_ids[i]);
                tex.bind(tex_bindings[i]);
            }

            for (auto &[mesh_id, instances] : mesh_group) {
                Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
                mesh.vao.vbo_instanced.set_data(
                    instances.data(), instances.size() * sizeof(MeshInstance));
                draw_elements_instanced(curr_shader, mesh.vao,
                                        instances.size());
                s_renderer.stats.draw_calls++;
            }
        }
    }

    GL_CALL(glFinish());
    t.stop();
    s_renderer.stats.base_pass_ms += t.elapsed_time_ms();
}

void shadow_pass_begin(const CameraData &camera, AssetPack &asset_pack) {
    s_asset_pack = &asset_pack;
    s_active_camera = &camera;

    s_renderer.dir_lights.clear();
    s_renderer.point_lights.clear();
    s_renderer.spot_lights.clear();
    s_renderer.draw_params.clear();

    for (auto &[shader_id, shader_group] : s_renderer.shader_render_group) {
        for (auto &[mat_id, mat_group] : shader_group) {
            for (auto &[mesh_id, instances] : mat_group)
                instances.clear();

            mat_group.clear();
        }

        shader_group.clear();
    }

    s_renderer.shader_render_group.clear();
}

void shadow_pass_end() {
    if (s_renderer.shader_render_group.empty() ||
        (s_renderer.dir_lights.empty() && s_renderer.point_lights.empty() &&
         s_renderer.spot_lights.empty()))
        return;

    Timer t;
    t.start();

    s_renderer.dir_lights_storage.bind();
    s_renderer.point_lights_storage.bind();
    s_renderer.spot_lights_storage.bind();

    int32_t count = s_renderer.dir_lights.size();
    int32_t offset = sizeof(int32_t) * 4;
    s_renderer.dir_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.dir_lights_storage.set_data(
        s_renderer.dir_lights.data(), count * sizeof(DirLightData), offset);

    count = s_renderer.point_lights.size();
    s_renderer.point_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.point_lights_storage.set_data(
        s_renderer.point_lights.data(), count * sizeof(PointLightData), offset);

    count = s_renderer.spot_lights.size();
    s_renderer.spot_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.spot_lights_storage.set_data(
        s_renderer.spot_lights.data(), count * sizeof(SpotLightData), offset);

    assert(s_renderer.shader_render_group.size() == 1 &&
           "More than 1 shader submitted for shadow pass");

    MaterialGroup &material_group = s_renderer.shader_render_group[0];
    assert(material_group.size() == 1 &&
           "More than 1 material submitted for shadow pass");

    MeshGroup &mesh_group = material_group[0];
    for (auto &[mesh_id, instances] : mesh_group) {
        Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
        mesh.vao.vbo_instanced.set_data(
            instances.data(), instances.size() * sizeof(MeshInstance));
    }

    s_renderer.shadow_fbo.bind();
    GL_CALL(glDrawBuffer(GL_NONE));
    GL_CALL(glCullFace(GL_FRONT));

    s_renderer.shadow_fbo.draw_to_depth_map(0);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));

    if (!s_renderer.dir_lights.empty()) {
        s_renderer.dirlight_shadow_shader.bind();
        for (auto &[mesh_id, instances] : mesh_group) {
            if (instances.empty())
                continue;

            Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
            draw_elements_instanced(s_renderer.dirlight_shadow_shader, mesh.vao,
                                    instances.size());
        }
    }

    s_renderer.shadow_fbo.draw_to_depth_map(1);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));

    if (!s_renderer.point_lights.empty()) {
        s_renderer.pointlight_shadow_shader.bind();
        for (auto &[mesh_id, instances] : mesh_group) {
            if (instances.empty())
                continue;

            Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
            draw_elements_instanced(s_renderer.pointlight_shadow_shader,
                                    mesh.vao, instances.size());
        }
    }

    s_renderer.shadow_fbo.draw_to_depth_map(2);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));

    if (!s_renderer.spot_lights.empty()) {
        s_renderer.spotlight_shadow_shader.bind();
        for (auto &[mesh_id, instances] : mesh_group) {
            if (instances.empty())
                continue;

            Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
            draw_elements_instanced(s_renderer.spotlight_shadow_shader,
                                    mesh.vao, instances.size());
        }
    }

    GL_CALL(glCullFace(GL_BACK));

    GL_CALL(glFinish());
    t.stop();
    s_renderer.stats.shadow_pass_ms += t.elapsed_time_ms();
}

void submit_mesh(const glm::mat4 &transform, AssetID mesh_id,
                 AssetID material_id, int32_t ent_id,
                 const DrawParams &params) {
    s_renderer.stats.instances++;

    Material &mat = s_asset_pack->materials.at(material_id);
    MaterialGroup &mat_grp = s_renderer.shader_render_group[mat.shader_id];
    MeshGroup &mesh_grp = mat_grp[material_id];
    std::vector<MeshInstance> &instances = mesh_grp[mesh_id];

    MeshInstance &instance = instances.emplace_back();
    instance.transform = transform;
    instance.entity_id = ent_id;

    std::vector<DrawParams> &draw_params = s_renderer.draw_params;
    for (int32_t i = 0; i < draw_params.size(); i++) {
        DrawParams &curr_params = draw_params[i];
        if (memcmp(&curr_params, &params, sizeof(DrawParams)) == 0) {
            instance.draw_params_idx = i;
            return;
        }
    }

    draw_params.push_back(params);
    instance.draw_params_idx = draw_params.size() - 1;
}

void submit_shadow_pass_mesh(const glm::mat4 &transform, AssetID mesh_id) {
    /* Only one shader in use at a time in shadow passes. */
    MaterialGroup &mat_grp = s_renderer.shader_render_group[0];
    MeshGroup &mesh_grp = mat_grp[0];
    std::vector<MeshInstance> &instances = mesh_grp[mesh_id];

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

    s_renderer.stats.dir_lights++;

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
    light_data.color = glm::vec4(light.color * light.intensity, 1.0f);
}

void submit_point_light(const glm::vec3 &position, const PointLight &light) {
    if (s_renderer.point_lights.size() >= MAX_POINT_LIGHTS)
        return;

    s_renderer.stats.point_lights++;

    float radius = light_radius(1.0f, light.linear, light.quadratic,
                                max_component(light.color));
    glm::mat4 proj =
        glm::perspective(glm::radians(91.0f), 1.0f, 0.1f, radius);

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

    s_renderer.stats.spot_lights++;

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

EnvMap create_envmap(const Texture &equirect) {
    EnvMap emap;
    emap.thumbnail = equirect;

    CubeTextureSpec spec;
    spec.format = TextureFormat::RGBA16F;
    spec.face_dim = equirect.spec.size.y / 2;
    spec.min_filter = GL_LINEAR_MIPMAP_LINEAR;
    spec.mag_filter = GL_LINEAR;
    spec.wrap = GL_CLAMP_TO_EDGE;
    spec.gen_mipmaps = true;

    emap.cube_map = CubeTexture::create(spec);

    spec.face_dim = 32;
    emap.irradiance_map = CubeTexture::create(spec);

    spec.face_dim = 128;
    emap.prefilter_map = CubeTexture::create(spec);

    glm::ivec3 groups;
    groups.x = (emap.cube_map.spec.face_dim + 15) / 16;
    groups.y = groups.x;
    groups.z = 1;

    equirect.bind();
    s_renderer.equirec_to_cubemap_shader.bind();
    for (int32_t face = 0; face < 6; face++) {
        emap.cube_map.bind_face_image(face, 0, 1, ImageAccess::WRITE);
        s_renderer.equirec_to_cubemap_shader.set_uniform_1i("u_face_idx", face);
        s_renderer.equirec_to_cubemap_shader.dispatch_compute(groups);
    }

    emap.cube_map.bind();
    GL_CALL(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));

    groups.x = (emap.irradiance_map.spec.face_dim + 15) / 16;
    groups.y = groups.x;
    groups.z = 1;

    s_renderer.cubemap_convolution_shader.bind();
    for (int32_t face = 0; face < 6; face++) {
        emap.irradiance_map.bind_face_image(face, 0, 1, ImageAccess::WRITE);
        s_renderer.cubemap_convolution_shader.set_uniform_1i("u_face_idx",
                                                             face);
        s_renderer.cubemap_convolution_shader.dispatch_compute(groups);
    }

    emap.irradiance_map.bind();
    GL_CALL(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));

    groups.x = (emap.prefilter_map.spec.face_dim + 15) / 16;
    groups.y = groups.x;
    groups.z = 1;

    emap.cube_map.bind();
    s_renderer.cubemap_prefilter_shader.bind();
    for (int32_t mip = 0; mip < emap.prefilter_map.spec.mips; mip++) {
        float roughness =
            (float)mip / (float)(emap.prefilter_map.spec.mips - 1);
        s_renderer.cubemap_prefilter_shader.set_uniform_1f("u_roughness",
                                                           roughness);

        for (int32_t face = 0; face < 6; face++) {
            emap.prefilter_map.bind_face_image(face, mip, 1,
                                               ImageAccess::WRITE);
            s_renderer.cubemap_prefilter_shader.set_uniform_1i("u_face_idx",
                                                               face);
            s_renderer.cubemap_prefilter_shader.dispatch_compute(groups);
        }
    }

    return emap;
}

void use_envmap(EnvMap &envmap) {
    s_envmap = &envmap;
}

SoftShadowProps &soft_shadow_props() {
    return s_renderer.soft_shadow_props;
}

TextureSlots texture_slots() {
    return s_renderer.slots;
}

RenderStats stats() {
    return s_renderer.stats;
}

void reset_stats() {
    memset(&s_renderer.stats, 0, sizeof(RenderStats));
}

void skybox(AssetID envmap_id) {
    CubeTexture &ct = s_asset_pack->env_maps.at(envmap_id).cube_map;
    ct.bind();

    GL_CALL(glDepthFunc(GL_LEQUAL));
    draw_arrays(s_renderer.cubemap_shader, s_renderer.cubemap_vao, 36);
    GL_CALL(glDepthFunc(GL_LESS));
}

void post_proc_combine() {
    s_renderer.bloom_texture.bind(1);

    glm::ivec3 groups;
    groups.x = (s_active_camera->viewport.x + 15) / 16;
    groups.y = (s_active_camera->viewport.y + 15) / 16;
    groups.z = 1;

    s_renderer.post_proc_combine_shader.dispatch_compute(groups);
}

void post_process() {
    TextureSpec new_spec = s_renderer.bloom_texture.spec;

    if (s_renderer.bloom_texture.spec.size !=
        glm::ivec2(s_active_camera->viewport))
        new_spec.size = glm::ivec2(s_active_camera->viewport);

    if (s_renderer.bloom_texture.spec.mips != s_active_camera->bloom_mip_radius)
        new_spec.mips = s_active_camera->bloom_mip_radius;

    if (memcmp(&new_spec, &s_renderer.bloom_texture.spec,
               sizeof(TextureSpec)) != 0) {
        s_renderer.bloom_texture.destroy();
        s_renderer.bloom_texture =
            Texture::create_storage(new_spec);
    }

    s_renderer.bloom_texture.clear_texture();
    s_renderer.bloom_texture.bind_image(0, 1, ImageAccess::WRITE);

    glm::ivec3 groups;
    groups.x = (s_active_camera->viewport.x + 8 - 1) / 8;
    groups.y = (s_active_camera->viewport.y + 8 - 1) / 8;
    groups.z = 1;

    s_renderer.bloom_filter.dispatch_compute(groups);

    s_renderer.bloom_texture.bind(1);

    s_renderer.bloom_downsampler.bind();
    for (int32_t i = 1; i < s_renderer.bloom_texture.spec.mips; i++) {
        s_renderer.bloom_texture.bind_image(i, 2, ImageAccess::WRITE);

        s_renderer.bloom_downsampler.set_uniform_1f("u_mip", i - 1);
        s_renderer.bloom_downsampler.dispatch_compute(groups);
    }

    s_renderer.bloom_upsampler.bind();
    for (int32_t i = s_renderer.bloom_texture.spec.mips - 1; i > 0; i--) {
        s_renderer.bloom_texture.bind_image(i - 1, 2, ImageAccess::READ_WRITE);

        s_renderer.bloom_upsampler.set_uniform_1f("u_mip", i);
        s_renderer.bloom_upsampler.dispatch_compute(groups);
    }
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
