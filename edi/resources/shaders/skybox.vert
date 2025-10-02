#version 460 core

#define CAMERA_BINDING ${CAMERA_BINDING}

layout (location = 0) in vec3 a_pos;

out vec3 local_pos;

layout (std140, binding = CAMERA_BINDING) uniform Camera {
    mat4 view_projection;
    mat4 projection;
    mat4 view;
    vec4 position;
    vec2 viewport;

    float exposure;
    float gamma;
    float near_clip;
    float far_clip;
    float fov;

    float bloom_strength;
    float bloom_threshold;
} u_camera;

void main() {
    local_pos = a_pos;

    vec4 pos = u_camera.projection * mat4(mat3(u_camera.view)) * vec4(a_pos, 1.0);
    gl_Position = pos.xyww;
}
