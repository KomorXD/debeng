#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "eng/renderer/opengl.hpp"
#include <glm/glm.hpp>

namespace eng::renderer {

struct CameraData {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 position;
    glm::vec2 viewport;
    
    float exposure;
    float gamma;
    float near_clip;
    float far_clip;
};

void opengl_msg_cb(unsigned source, unsigned type, unsigned id,
                   unsigned severity, int length, const char *msg,
                   const void *user_param);

bool init();

void shutdown();

void scene_begin(const CameraData &camera);
void scene_end();

void submit_quad(const glm::vec3 &position);
void submit_cube(const glm::vec3 &position);

void draw_to_screen_quad();

void draw_arrays(const Shader &shader, const VertexArray &vao, uint32_t count);
void draw_indexed_instanced(const Shader &shader, const VertexArray &vao,
                            uint32_t count);

} // namespace eng

#endif
