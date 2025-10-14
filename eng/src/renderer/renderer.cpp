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
#include <strings.h>
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

    Shader depth_pass_shader;
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
    int32_t dir_lights_allocated = MIN_DIR_LIGHTS_STORAGE;
    std::vector<DirLightData> dir_lights;

    ShaderStorage point_lights_storage;
    int32_t point_lights_allocated = MIN_POINT_LIGHTS_STORAGE;
    std::vector<PointLightData> point_lights;

    ShaderStorage spot_lights_storage;
    int32_t spot_lights_allocated = MIN_SPOT_LIGHTS_STORAGE;
    std::vector<SpotLightData> spot_lights;

    ShaderStorage visible_indicies_storage;
    Shader light_culling_shader;

    Framebuffer shadow_fbo;
    Shader dirlight_shadow_shader;
    Shader pointlight_shadow_shader;
    Shader spotlight_shadow_shader;

    GLuint random_offset_tex_id = 0;
    UniformBuffer soft_shadow_uni_buffer;
    SoftShadowProps soft_shadow_props;
    SoftShadowProps cached_soft_shadow_props;

    ShaderGroup shader_render_group;
};

static Renderer s_renderer{};
static AssetPack *s_asset_pack{};
static const CameraData *s_active_camera{};
static EnvMap *s_envmap{};
static Framebuffer *s_target_fbo{};

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
    s_renderer.slots.orm = 2;
    s_renderer.slots.emission_map = 4;
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

    s_renderer.depth_pass_shader = Shader::create();
    {
        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/depth.vert";
        spec.vertex_shader.replacements = {
            {"${CAMERA_BINDING}", std::to_string(CAMERA_BINDING)}};
        spec.fragment_shader.path = "resources/shaders/depth.frag";

        assert(s_renderer.depth_pass_shader.build(spec) &&
               "Failed to build depth pass-thourgh shader");
    }

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

    size = MIN_DIR_LIGHTS_STORAGE * sizeof(DirLightData) + sizeof(int32_t) * 4;
    s_renderer.dir_lights_storage = ShaderStorage::create(nullptr, size);
    s_renderer.dir_lights_storage.bind_buffer_range(DIR_LIGHTS_BINDING, 0,
                                                    size);

    size =
        MIN_POINT_LIGHTS_STORAGE * sizeof(PointLightData) + sizeof(int32_t) * 4;
    s_renderer.point_lights_storage = ShaderStorage::create(nullptr, size);
    s_renderer.point_lights_storage.bind_buffer_range(POINT_LIGHTS_BINDING, 0,
                                                      size);

    size =
        MIN_SPOT_LIGHTS_STORAGE * sizeof(SpotLightData) + sizeof(int32_t) * 4;
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
        constexpr glm::ivec2 ASSUMED_SCREEN_DIM = glm::ivec2(1920, 1080);
        constexpr glm::ivec2 ASSUMED_TILE_DIM = glm::ivec2(16, 16);
        constexpr int32_t ASSUMED_TILE_COUNT =
            (ASSUMED_SCREEN_DIM.x / ASSUMED_TILE_DIM.x) *
            (ASSUMED_SCREEN_DIM.y / ASSUMED_TILE_DIM.y);
        constexpr int32_t ASSUMED_MAX_LIGHTS_PER_TILE = MAX_POINT_LIGHTS;
        constexpr int32_t ASSUMED_BUFFER_SIZE =
            ASSUMED_TILE_COUNT * ASSUMED_MAX_LIGHTS_PER_TILE * sizeof(int32_t);

        char *data = new char[ASSUMED_BUFFER_SIZE];
        bzero(data, ASSUMED_BUFFER_SIZE);
        /* ~2Mb */
        s_renderer.visible_indicies_storage =
            ShaderStorage::create(data, ASSUMED_BUFFER_SIZE);
        s_renderer.visible_indicies_storage.bind_buffer_range(
            VISIBLE_INDICES_BINDING, 0, ASSUMED_BUFFER_SIZE);
        delete[] data;
    }

    {
        s_renderer.light_culling_shader = Shader::create();
        bool success = s_renderer.light_culling_shader.build_compute(
            {.path = "resources/shaders/light_cull.comp",
             .replacements = {
                {
                    "${VISIBLE_INDICES_BINDING}",
                    std::to_string(VISIBLE_INDICES_BINDING)
                },
                {
                    "${POINT_LIGHTS_BINDING}",
                    std::to_string(POINT_LIGHTS_BINDING)
                },
                {
                    "${MAX_POINT_LIGHTS}",
                    std::to_string(MAX_POINT_LIGHTS)
                },
                {
                    "${CAMERA_BINDING}",
                    std::to_string(CAMERA_BINDING)
                }
            }
        });

        assert(success && "Failed to build light culling shader");
    }

    {
        s_renderer.shadow_fbo = Framebuffer::create();
        s_renderer.shadow_fbo.bind();

        DepthAttachmentSpec spec;
        spec.type = DepthAttachmentType::DEPTH;
        spec.tex_type = TextureType::TEX_2D_ARRAY_SHADOW;
        spec.size = {2048, 2048};
        spec.layers = MIN_DIR_LIGHTS_STORAGE * CASCADES_COUNT;
        s_renderer.shadow_fbo.add_depth_attachment(spec);

        spec.size = {512, 512};
        spec.layers = MIN_POINT_LIGHTS_STORAGE * 6;
        s_renderer.shadow_fbo.add_depth_attachment(spec);

        spec.layers = MIN_SPOT_LIGHTS_STORAGE;
        s_renderer.shadow_fbo.add_depth_attachment(spec);

        s_renderer.shadow_fbo.draw_to_depth_attachment(0);
        assert(s_renderer.shadow_fbo.is_complete() &&
               "Incomplete shadow framebuffer");
    }

    {
        s_renderer.dirlight_shadow_shader = Shader::create();

        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/shadows/depth-shadow.vert";
        spec.fragment_shader.path = "resources/shaders/depth.frag";
        spec.geometry_shader = {
            .path = "resources/shaders/shadows/dirlight.geom",
            .replacements = {
                {
                    "${DIR_LIGHTS_BINDING}",
                    std::to_string(renderer::DIR_LIGHTS_BINDING)
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
        spec.vertex_shader.path = "resources/shaders/shadows/depth-shadow.vert";
        spec.fragment_shader.path = "resources/shaders/depth.frag";
        spec.geometry_shader = {
            .path = "resources/shaders/shadows/pointlight.geom",
            .replacements = {
                {
                    "${POINT_LIGHTS_BINDING}",
                    std::to_string(renderer::POINT_LIGHTS_BINDING)
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
        spec.vertex_shader.path = "resources/shaders/shadows/depth-shadow.vert";
        spec.fragment_shader.path = "resources/shaders/depth.frag";
        spec.geometry_shader = {
            .path = "resources/shaders/shadows/spotlight.geom",
            .replacements = {
                {
                    "${SPOT_LIGHTS_BINDING}",
                    std::to_string(renderer::SPOT_LIGHTS_BINDING)
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

    return true;
}

void shutdown() {
    s_renderer.camera_uni_buffer.destroy();
    s_renderer.dir_lights_storage.destroy();
    s_renderer.point_lights_storage.destroy();
    s_renderer.spot_lights_storage.destroy();
    s_renderer.soft_shadow_uni_buffer.destroy();

    s_renderer.light_culling_shader.destroy();
    s_renderer.visible_indicies_storage.destroy();

    s_renderer.shadow_fbo.destroy();
    s_renderer.dirlight_shadow_shader.destroy();
    s_renderer.spotlight_shadow_shader.destroy();
    s_renderer.pointlight_shadow_shader.destroy();

    s_renderer.depth_pass_shader.destroy();
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

void scene_begin(const CameraData &camera, AssetPack &asset_pack,
                 Framebuffer &target_fbo) {
    s_asset_pack = &asset_pack;
    s_active_camera = &camera;
    s_target_fbo = &target_fbo;

    assert(s_asset_pack && "Empty asset pack object");

    s_renderer.dir_lights.clear();
    s_renderer.point_lights.clear();
    s_renderer.spot_lights.clear();

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

static int32_t try_realloc_light_storage(int32_t needed_count,
                                         int32_t curr_count,
                                         size_t element_size,
                                         ShaderStorage &storage,
                                         int32_t binding) {
    int32_t new_cap = curr_count * element_size;
    int32_t needed_cap = needed_count * element_size;

    if (needed_count > curr_count) {
        while (new_cap < needed_cap)
            new_cap = (int32_t)(new_cap * 1.5f);
    } else if (needed_count < (int32_t)(curr_count * 0.66f)) {
        while ((int32_t)(new_cap * 0.66f) > needed_cap)
            new_cap = (int32_t)(new_cap * 0.66f);
    } else
        return curr_count;

    storage.bind();
    storage.realloc(new_cap + sizeof(int32_t) * 4);
    storage.bind_buffer_range(binding, 0, new_cap + sizeof(int32_t) * 4);

    return new_cap / element_size;
}

void scene_end() {
    Timer t;
    t.start();

    s_target_fbo->bind();
    s_target_fbo->draw_to_depth_attachment(0);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    GL_CALL(glDrawBuffer(GL_NONE));
    GL_CALL(glDepthFunc(GL_LESS));

    for (auto &[shader_id, material_group] : s_renderer.shader_render_group) {
        for (auto &[mat_id, mesh_group] : material_group) {
            for (auto &[mesh_id, instances] : mesh_group) {
                Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
                mesh.vao.vbo_instanced.set_data(
                    instances.data(), instances.size() * sizeof(MeshInstance));
                draw_elements_instanced(s_renderer.depth_pass_shader, mesh.vao,
                                        instances.size());
            }
        }
    }

    int32_t count = s_renderer.dir_lights.size();
    s_renderer.dir_lights_allocated = try_realloc_light_storage(
        glm::max(count, MIN_DIR_LIGHTS_STORAGE),
        s_renderer.dir_lights_allocated, sizeof(DirLightData),
        s_renderer.dir_lights_storage, DIR_LIGHTS_BINDING);

    int32_t offset = sizeof(int32_t) * 4;
    s_renderer.dir_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.dir_lights_storage.set_data(
        s_renderer.dir_lights.data(), count * sizeof(DirLightData), offset);


    count = s_renderer.point_lights.size();
    s_renderer.point_lights_allocated = try_realloc_light_storage(
        glm::max(count, MIN_POINT_LIGHTS_STORAGE),
        s_renderer.point_lights_allocated, sizeof(PointLightData),
        s_renderer.point_lights_storage, POINT_LIGHTS_BINDING);

    s_renderer.point_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.point_lights_storage.set_data(
        s_renderer.point_lights.data(), count * sizeof(PointLightData), offset);


    count = s_renderer.spot_lights.size();
    s_renderer.spot_lights_allocated = try_realloc_light_storage(
        glm::max(count, MIN_SPOT_LIGHTS_STORAGE),
        s_renderer.spot_lights_allocated, sizeof(SpotLightData),
        s_renderer.spot_lights_storage, SPOT_LIGHTS_BINDING);

    s_renderer.spot_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.spot_lights_storage.set_data(
        s_renderer.spot_lights.data(), count * sizeof(SpotLightData), offset);

    s_renderer.dir_lights_storage.bind();
    s_renderer.point_lights_storage.bind();
    s_renderer.spot_lights_storage.bind();

    s_renderer.visible_indicies_storage.bind();
    s_target_fbo->bind_depth_attachment(0);

    glm::ivec3 groups;
    groups.x = (s_active_camera->viewport.x + 15) / 16;
    groups.y = (s_active_camera->viewport.y + 15) / 16;
    groups.z = 1;
    s_renderer.light_culling_shader.dispatch_compute(groups);

    s_target_fbo->draw_to_color_attachment(0, 0);
    s_target_fbo->draw_to_color_attachment(1, 1);
    s_target_fbo->fill_color_draw_buffers();
    GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
    GL_CALL(glClearColor(0.33f, 0.33f, 0.33f, 1.0f));
    GL_CALL(glDepthFunc(GL_EQUAL));
    s_target_fbo->clear_color_attachment(1);


    s_renderer.shadow_fbo.bind_depth_attachment(
        0, s_renderer.slots.dir_csm_shadowmaps);
    s_renderer.shadow_fbo.bind_depth_attachment(
        1, s_renderer.slots.point_lights_shadowmaps);
    s_renderer.shadow_fbo.bind_depth_attachment(
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
            curr_shader.try_set_uniform_4f(
                "u_material.emission_and_ao",
                glm::vec4(mat.emission_color * mat.emission_factor, mat.ao));
            curr_shader.try_set_uniform_1f("u_material.roughness",
                                           mat.roughness);
            curr_shader.try_set_uniform_1f("u_material.metallic", mat.metallic);

            std::array<AssetID, 4> tex_ids = {
                mat.albedo_texture_id, mat.normal_texture_id,
                mat.orm_texture_id, mat.emission_texture_id};
            std::array<int32_t, 4> tex_bindings = {
                s_renderer.slots.albedo, s_renderer.slots.normal,
                s_renderer.slots.orm, s_renderer.slots.emission_map};

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

    GL_CALL(glDepthFunc(GL_LESS));
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

static void try_change_shadow_layers(Framebuffer &fbo,
                                     int32_t depth_attach_index,
                                     int32_t expected_layers) {
    DepthAttachmentSpec spec = fbo.depth_attachments[depth_attach_index].spec;
    if (spec.layers == expected_layers)
        return;

    spec.layers = expected_layers;
    fbo.rebuild_depth_attachment(depth_attach_index, spec);
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
    s_renderer.dir_lights_allocated = try_realloc_light_storage(
        glm::max(count, MIN_DIR_LIGHTS_STORAGE),
        s_renderer.dir_lights_allocated, sizeof(DirLightData),
        s_renderer.dir_lights_storage, DIR_LIGHTS_BINDING);

    int32_t offset = sizeof(int32_t) * 4;
    s_renderer.dir_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.dir_lights_storage.set_data(
        s_renderer.dir_lights.data(), count * sizeof(DirLightData), offset);

    try_change_shadow_layers(s_renderer.shadow_fbo, 0,
                             s_renderer.dir_lights_allocated * CASCADES_COUNT);


    count = s_renderer.point_lights.size();
    s_renderer.point_lights_allocated = try_realloc_light_storage(
        glm::max(count, MIN_POINT_LIGHTS_STORAGE),
        s_renderer.point_lights_allocated, sizeof(PointLightData),
        s_renderer.point_lights_storage, POINT_LIGHTS_BINDING);

    s_renderer.point_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.point_lights_storage.set_data(
        s_renderer.point_lights.data(), count * sizeof(PointLightData), offset);

    try_change_shadow_layers(s_renderer.shadow_fbo, 1,
                             s_renderer.point_lights_allocated * 6);


    count = s_renderer.spot_lights.size();
    s_renderer.spot_lights_allocated = try_realloc_light_storage(
        glm::max(count, MIN_SPOT_LIGHTS_STORAGE),
        s_renderer.spot_lights_allocated, sizeof(SpotLightData),
        s_renderer.spot_lights_storage, SPOT_LIGHTS_BINDING);

    s_renderer.spot_lights_storage.set_data(&count, sizeof(int32_t));
    s_renderer.spot_lights_storage.set_data(
        s_renderer.spot_lights.data(), count * sizeof(SpotLightData), offset);

    try_change_shadow_layers(s_renderer.shadow_fbo, 2,
                             s_renderer.spot_lights_allocated);


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

    s_renderer.shadow_fbo.draw_to_depth_attachment(0);
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

    s_renderer.shadow_fbo.draw_to_depth_attachment(1);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));

    if (!s_renderer.point_lights.empty()) {
        s_renderer.pointlight_shadow_shader.bind();

        int32_t passes = (s_renderer.point_lights.size() /
                          s_renderer.gpu.max_geom_invocations) +
                         1;
        for (int32_t pass = 0; pass < passes; pass++) {
            s_renderer.pointlight_shadow_shader.set_uniform_1i(
                "u_offset", pass * s_renderer.gpu.max_geom_invocations);

            for (auto &[mesh_id, instances] : mesh_group) {
                if (instances.empty())
                    continue;

                Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
                draw_elements_instanced(s_renderer.pointlight_shadow_shader,
                                        mesh.vao, instances.size());
            }
        }
    }

    s_renderer.shadow_fbo.draw_to_depth_attachment(2);
    GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));

    if (!s_renderer.spot_lights.empty()) {
        s_renderer.spotlight_shadow_shader.bind();

        int32_t passes = (s_renderer.spot_lights.size() /
                          s_renderer.gpu.max_geom_invocations) +
                         1;
        for (int32_t pass = 0; pass < passes; pass++) {
            s_renderer.spotlight_shadow_shader.set_uniform_1i(
                "u_offset", pass * s_renderer.gpu.max_geom_invocations);

            for (auto &[mesh_id, instances] : mesh_group) {
                if (instances.empty())
                    continue;

                Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
                draw_elements_instanced(s_renderer.spotlight_shadow_shader,
                                        mesh.vao, instances.size());
            }
        }
    }

    GL_CALL(glCullFace(GL_BACK));

    GL_CALL(glFinish());
    t.stop();
    s_renderer.stats.shadow_pass_ms += t.elapsed_time_ms();
}

static MeshAABB world_space_bb(Mesh &mesh, const glm::mat4 &transform) {
    glm::vec3 world_min( FLT_MAX);
    glm::vec3 world_max(-FLT_MAX);

    MeshAABB &local = mesh.local_bb;

    std::array<glm::vec3, 8> corners = {
        glm::vec3{local.min.x, local.min.y, local.min.z},
        glm::vec3{local.max.x, local.min.y, local.min.z},
        glm::vec3{local.min.x, local.max.y, local.min.z},
        glm::vec3{local.max.x, local.max.y, local.min.z},
        glm::vec3{local.min.x, local.min.y, local.max.z},
        glm::vec3{local.max.x, local.min.y, local.max.z},
        glm::vec3{local.min.x, local.max.y, local.max.z},
        glm::vec3{local.max.x, local.max.y, local.max.z}
    };

    for (int32_t i = 0; i < corners.size(); i++) {
        glm::vec3 w = glm::vec3(transform * glm::vec4(corners[i], 1.0f));
        world_min = glm::min(world_min, w);
        world_max = glm::max(world_max, w);
    }

    return {world_min, world_max};
}

struct Plane {
    glm::vec3 normal;
    float d;
};

std::array<Plane, 6> extract_frustm_planes(const glm::mat4 &mat) {
    std::array<Plane, 6> planes;

    // Left
    planes[0].normal.x = mat[0][3] + mat[0][0];
    planes[0].normal.y = mat[1][3] + mat[1][0];
    planes[0].normal.z = mat[2][3] + mat[2][0];
    planes[0].d        = mat[3][3] + mat[3][0];

    // Right
    planes[1].normal.x = mat[0][3] - mat[0][0];
    planes[1].normal.y = mat[1][3] - mat[1][0];
    planes[1].normal.z = mat[2][3] - mat[2][0];
    planes[1].d        = mat[3][3] - mat[3][0];

    // Bottom
    planes[2].normal.x = mat[0][3] + mat[0][1];
    planes[2].normal.y = mat[1][3] + mat[1][1];
    planes[2].normal.z = mat[2][3] + mat[2][1];
    planes[2].d        = mat[3][3] + mat[3][1];

    // Top
    planes[3].normal.x = mat[0][3] - mat[0][1];
    planes[3].normal.y = mat[1][3] - mat[1][1];
    planes[3].normal.z = mat[2][3] - mat[2][1];
    planes[3].d        = mat[3][3] - mat[3][1];

    // Near
    planes[4].normal.x = mat[0][3] + mat[0][2];
    planes[4].normal.y = mat[1][3] + mat[1][2];
    planes[4].normal.z = mat[2][3] + mat[2][2];
    planes[4].d        = mat[3][3] + mat[3][2];

    // Far
    planes[5].normal.x = mat[0][3] - mat[0][2];
    planes[5].normal.y = mat[1][3] - mat[1][2];
    planes[5].normal.z = mat[2][3] - mat[2][2];
    planes[5].d        = mat[3][3] - mat[3][2];

    for (int i = 0; i < 6; i++) {
        float inv_len = 1.0f / glm::length(planes[i].normal);
        planes[i].normal *= inv_len;
        planes[i].d *= inv_len;
    }

    return planes;
}

void submit_mesh(const glm::mat4 &transform, AssetID mesh_id,
                 AssetID material_id, int32_t ent_id) {
    s_renderer.stats.submitted_instances++;

    Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
    MeshAABB world_bb = world_space_bb(mesh, transform);

    std::array<Plane, 6> camera_planes =
        extract_frustm_planes(s_active_camera->view_projection);
    for (int32_t i = 0; i < camera_planes.size(); i++) {
        glm::vec3 n = camera_planes[i].normal;
        float d = camera_planes[i].d;

        glm::vec3 pos = world_bb.min;
        if (n.x >= 0)   pos.x = world_bb.max.x;
        if (n.y >= 0)   pos.y = world_bb.max.y;
        if (n.z >= 0)   pos.z = world_bb.max.z;

        if (glm::dot(n, pos) + d < 0.0f)
            return;
    }

    s_renderer.stats.accepted_instances++;

    Material &mat = s_asset_pack->materials.at(material_id);
    MaterialGroup &mat_grp = s_renderer.shader_render_group[mat.shader_id];
    MeshGroup &mesh_grp = mat_grp[material_id];
    std::vector<MeshInstance> &instances = mesh_grp[mesh_id];

    MeshInstance &instance = instances.emplace_back();
    instance.transform = transform;
    instance.entity_id = ent_id;
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

void submit_shadow_mesh(const glm::mat4 &transform, AssetID mesh_id) {
    Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
    MeshAABB world_bb = world_space_bb(mesh, transform);

    bool visible_to_any = false;
    for (const PointLightData &pl : s_renderer.point_lights) {
        glm::vec3 position = glm::vec3(pl.position_and_radius);
        glm::vec3 closest = glm::clamp(position, world_bb.min, world_bb.max);

        float dist2 = glm::length2(position - closest);
        float radius = pl.position_and_radius.w;
        if (dist2 <= radius * radius) {
            visible_to_any = true;
            break;
        }
    }

    if (!visible_to_any) {
        for (const SpotLightData &sl : s_renderer.spot_lights) {
            glm::vec3 position = glm::vec3(sl.pos_and_cutoff);
            glm::vec3 closest =
                glm::clamp(position, world_bb.min, world_bb.max);

            float dist2 = glm::length2(position - closest);
            float radius = sl.color_and_distance.w;
            if (dist2 <= radius * radius) {
                visible_to_any = true;
                break;
            }
        }
    }

    if (!visible_to_any)
        return;

    s_renderer.stats.shadow_meshes_rendered++;

    /* Only one shader in use at a time in shadow passes. */
    MaterialGroup &mat_grp = s_renderer.shader_render_group[0];
    MeshGroup &mesh_grp = mat_grp[0];
    std::vector<MeshInstance> &instances = mesh_grp[mesh_id];

    MeshInstance &instance = instances.emplace_back();
    instance.transform = transform;
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

    s_renderer.stats.submitted_point_lights++;

    std::array<Plane, 6> camera_planes =
        extract_frustm_planes(s_active_camera->view_projection);
    for (int32_t i = 0; i < camera_planes.size(); i++) {
        Plane &plane = camera_planes[i];
        float dist = glm::dot(plane.normal, position) + plane.d + light.radius;
        if (dist <= 0.0f)
            return;
    }

    s_renderer.stats.accepted_point_lights++;

    glm::mat4 proj =
        glm::perspective(glm::radians(91.0f), 1.0f, 0.1f, light.radius);

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
    light_data.position_and_radius =
        glm::vec4(position, light.radius);
    light_data.color = light.color * light.intensity;
}

void submit_spot_light(const GlobalTransform &transform,
                       const SpotLight &light) {
    if (s_renderer.spot_lights.size() >= MAX_SPOT_LIGHTS)
        return;

    s_renderer.stats.submitted_spot_lights++;

    std::array<Plane, 6> camera_planes =
        extract_frustm_planes(s_active_camera->view_projection);
    for (int32_t i = 0; i < camera_planes.size(); i++) {
        Plane &plane = camera_planes[i];
        float dist = glm::dot(plane.normal, transform.position) + plane.d +
                     light.distance;
        if (dist <= 0.0f)
            return;
    }

    s_renderer.stats.accepted_spot_lights++;

    glm::vec3 dir = glm::toMat3(glm::quat(transform.rotation)) *
                    glm::vec3(0.0f, 0.0f, -1.0f);

    glm::mat4 proj = glm::perspective(glm::radians(2.0f * light.cutoff), 1.0f,
                                      0.1f, light.distance);
    glm::mat4 view = glm::lookAt(transform.position, transform.position + dir,
                                 glm::vec3(0.0f, 1.0f, 0.0f));

    SpotLightData &light_data = s_renderer.spot_lights.emplace_back();
    light_data.light_space_mat = proj * view;
    light_data.pos_and_cutoff =
        glm::vec4(transform.position, glm::cos(glm::radians(light.cutoff)));
    light_data.dir_and_outer_cutoff = glm::vec4(
        dir, glm::cos(glm::radians(light.cutoff - light.edge_smoothness)));
    light_data.color_and_distance =
        glm::vec4(light.color * light.intensity, light.distance);
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
