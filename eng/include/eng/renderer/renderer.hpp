#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "eng/renderer/opengl.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include <glm/glm.hpp>

namespace eng::renderer {

constexpr int32_t CAMERA_BINDING = 0;
constexpr int32_t DIR_LIGHTS_BINDING = 1;
constexpr int32_t POINT_LIGHTS_BINDING = 2;
constexpr int32_t SPOT_LIGHTS_BINDING = 3;
constexpr int32_t SOFT_SHADOW_PROPS_BINDING = 4;
constexpr int32_t VISIBLE_INDICES_BINDING = 5;

constexpr int32_t MAX_MESH_INSTANCES = 256;

constexpr int32_t MAX_DIR_LIGHTS = 8;
constexpr int32_t MIN_DIR_LIGHTS_STORAGE = 2;

constexpr int32_t MAX_POINT_LIGHTS = 256;
constexpr int32_t MIN_POINT_LIGHTS_STORAGE = 32;

constexpr int32_t MAX_SPOT_LIGHTS = 128;
constexpr int32_t MIN_SPOT_LIGHTS_STORAGE = 32;

constexpr int32_t CASCADES_COUNT = 5;

struct TextureSlots {
    int32_t albedo{};
    int32_t normal{};
    int32_t orm{};
    int32_t irradiance_map{};
    int32_t prefilter_map{};
    int32_t prefilter_mips{};
    int32_t brdf_lut{};
    int32_t dir_csm_shadowmaps{};
    int32_t point_lights_shadowmaps{};
    int32_t spot_lights_shadowmaps{};
    int32_t random_offsets_texture{};
};

struct alignas(16) CameraData {
    glm::mat4 view_projection;
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec4 position;
    glm::vec2 viewport;

    float exposure;
    float gamma;
    float near_clip;
    float far_clip;
    float fov;

    float bloom_strength;
    float bloom_threshold;
    int32_t bloom_mip_radius;
};

struct alignas(16) DirLightData {
    std::array<glm::mat4, CASCADES_COUNT> cascade_mats;
    glm::vec4 direction;
    glm::vec4 color;
};

struct alignas(16) PointLightData {
    std::array<glm::mat4, 6> light_space_matrices;
    glm::vec4 position_and_radius;
    glm::vec3 color;
};

struct alignas(16) SpotLightData {
    glm::mat4 light_space_mat;
    glm::vec4 pos_and_cutoff;
    glm::vec4 dir_and_outer_cutoff;
    glm::vec4 color_and_distance;
};

struct alignas(16) SoftShadowProps {
    int32_t offsets_tex_size = 16;
    int32_t offsets_filter_size = 8;
    float offset_radius = 3.0f;
};

struct alignas(16) MaterialData {
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec2 tiling_factor = glm::vec2(1.0f);
    glm::vec2 texture_offset = glm::vec2(0.0f);

    float roughness = -1.0f;
    float metallic = -1.0f;
    float ao = -1.0f;
};

struct RenderStats {
    float shadow_pass_ms{};
    float base_pass_ms{};

    int32_t dir_lights{};

    int32_t submitted_point_lights{};
    int32_t accepted_point_lights{};

    int32_t submitted_spot_lights{};
    int32_t accepted_spot_lights{};

    int32_t shadow_meshes_rendered{};

    int32_t submitted_instances{};
    int32_t accepted_instances{};

    int32_t draw_calls{};
};

void opengl_msg_cb(unsigned source, unsigned type, unsigned id,
                   unsigned severity, int length, const char *msg,
                   const void *user_param);

[[nodiscard]] bool init();

void shutdown();

void scene_begin(const CameraData &camera, AssetPack &asset_pack,
                 Framebuffer &target_fbo);
void scene_end();

void shadow_pass_begin(const CameraData &camera, AssetPack &asset_pack);
void shadow_pass_end();

void submit_mesh(const glm::mat4 &transform, AssetID mesh_id,
                 AssetID material_id, int32_t ent_id);
void submit_shadow_mesh(const glm::mat4 &transform, AssetID mesh_id);

void submit_dir_light(const glm::vec3 &rotation, const DirLight &light);
void submit_point_light(const glm::vec3 &position, const PointLight &light);
void submit_spot_light(const Transform &transform, const SpotLight &light);

EnvMap create_envmap(const Texture &equirect);
void use_envmap(EnvMap &envmap);

SoftShadowProps &soft_shadow_props();
TextureSlots texture_slots();

RenderStats stats();
void reset_stats();

void skybox(AssetID envmap_id);
void post_proc_combine();
void post_process();

void draw_arrays(const Shader &shader, const VertexArray &vao,
                 uint32_t vertices_count);
void draw_arrays_instanced(const Shader &shader, const VertexArray &vao,
                           uint32_t vertices_count, uint32_t instances_count);
void draw_elements(const Shader &shader, const VertexArray &vao);
void draw_elements_instanced(const Shader &shader, const VertexArray &vao,
                             uint32_t instances_count);

} // namespace eng

#endif
