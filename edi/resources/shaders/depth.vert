#version 430 core

#define CAMERA_BINDING ${CAMERA_BINDING}

layout (location = 0) in vec3 a_pos;
layout (location = 5) in mat4 a_transform;

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
    gl_Position = u_camera.view_projection * a_transform * vec4(a_pos, 1.0);
}
