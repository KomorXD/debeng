#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "eng/renderer/opengl.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include <glm/glm.hpp>

namespace eng::renderer {

constexpr int32_t CAMERA_BINDING = 0;
constexpr int32_t POINT_LIGHTS_BINDING = 1;
constexpr int32_t DIR_LIGHTS_BINDING = 2;
constexpr int32_t SPOT_LIGHTS_BINDING = 3;
constexpr int32_t MATERIALS_BINDING = 4;

constexpr int32_t MAX_MESH_INSTANCES = 128;
constexpr int32_t MAX_POINT_LIGHTS = 128;
constexpr int32_t MAX_DIR_LIGHTS = 4;
constexpr int32_t MAX_SPOT_LIGHTS = 128;
constexpr int32_t MAX_MATERIALS = 128;
constexpr int32_t MAX_TEXTURES = 16;

struct CameraData {
    glm::mat4 view_projection;
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec4 position;
    glm::vec2 viewport;

    float exposure;
    float gamma;
    float near_clip;
    float far_clip;
};

struct PointLightData {
    glm::vec4 position_and_linear;
    glm::vec4 color_and_quadratic;
};

struct DirLightData {
    glm::vec4 direction;
    glm::vec4 color;
};

struct SpotLightData {
    glm::mat4 light_space_mat;
    glm::vec4 pos_and_cutoff;
    glm::vec4 dir_and_outer_cutoff;
    glm::vec4 color_and_linear;
    float quadratic;

    glm::vec3 padding;
};

struct MaterialData {
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec2 tiling_factor = glm::vec2(1.0f);
    glm::vec2 texture_offset = glm::vec2(0.0f);

    int32_t albedo_idx = -1;
    int32_t normal_idx = -1;

    int32_t roughness_idx = -1;
    float roughness = -1.0f;

    int32_t metallic_idx = -1;
    float metallic = -1.0f;

    int32_t ao_idx = -1;
    float ao = -1.0f;
};

void opengl_msg_cb(unsigned source, unsigned type, unsigned id,
                   unsigned severity, int length, const char *msg,
                   const void *user_param);

[[nodiscard]] bool init();

void shutdown();

void scene_begin(const CameraData &camera, AssetPack &asset_pack);
void scene_end();

void submit_mesh(const glm::mat4 &transform, AssetID mesh_id,
                 AssetID material_id, int32_t ent_id, float color_sens = 1.0f);

void submit_point_light(const glm::vec3 &position, const PointLight &light);
void submit_dir_light(const glm::vec3 &rotation, const DirLight &light);
void submit_spot_light(const Transform &transform, const SpotLight &light);

void draw_to_screen_quad();

void draw_arrays(const Shader &shader, const VertexArray &vao,
                 uint32_t vertices_count);
void draw_arrays_instanced(const Shader &shader, const VertexArray &vao,
                           uint32_t vertices_count, uint32_t instances_count);
void draw_elements(const Shader &shader, const VertexArray &vao);
void draw_elements_instanced(const Shader &shader, const VertexArray &vao,
                             uint32_t instances_count);

enum class RenderPassMode {
    BASE,
    FLAT,

    COUNT
};

} // namespace eng

#endif
