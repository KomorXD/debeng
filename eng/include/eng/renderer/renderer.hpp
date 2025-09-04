#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "eng/renderer/opengl.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include <glm/glm.hpp>

namespace eng::renderer {

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

struct MaterialData {
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec2 tiling_factor = glm::vec2(1.0f);
    glm::vec2 texture_offset = glm::vec2(0.0f);

    int32_t albedo_idx = -1;

    glm::vec3 padding;
};

void opengl_msg_cb(unsigned source, unsigned type, unsigned id,
                   unsigned severity, int length, const char *msg,
                   const void *user_param);

[[nodiscard]] bool init();

void shutdown();

void scene_begin(const CameraData &camera, AssetPack &asset_pack);
void scene_end();

void submit_mesh(const glm::mat4 &transform, AssetID mesh_id,
                 AssetID material_id);
void submit_quad(const glm::vec3 &position);
void submit_cube(const glm::vec3 &position);

void submit_point_light(const glm::vec3 &position, const PointLight &light);

void draw_to_screen_quad();

void draw_arrays(const Shader &shader, const VertexArray &vao,
                 uint32_t vertices_count);
void draw_arrays_instanced(const Shader &shader, const VertexArray &vao,
                           uint32_t vertices_count, uint32_t instances_count);
void draw_elements(const Shader &shader, const VertexArray &vao);
void draw_elements_instanced(const Shader &shader, const VertexArray &vao,
                             uint32_t instances_count);

} // namespace eng

#endif
