#version 430 core

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_tangent;
layout (location = 3) in vec3 a_bitangent;
layout (location = 4) in vec2 a_texture_uv;
layout (location = 5) in mat4 a_transform;

layout (std140, binding = 0) uniform Camera {
    mat4 view_projection;
    mat4 projection;
    mat4 view;
    vec4 position;
    vec2 viewport;

    float exposure;
    float gamma;
    float near_clip;
    float far_clip;
} u_camera;

out vec2 tex_pos;

void main() {
    gl_Position = u_camera.view_projection * a_transform * vec4(a_pos, 1.0);
    tex_pos = a_texture_uv;
}
