#version 430 core

#define CAMERA_BINDING ${CAMERA_BINDING}

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_tangent;
layout (location = 3) in vec3 a_bitangent;
layout (location = 4) in vec2 a_texture_uv;
layout (location = 5) in mat4 a_transform;
layout (location = 9) in float a_material_idx;
layout (location = 10) in float a_ent_id;

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
} u_camera;

out VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    mat3 TBN;
    vec2 texture_uv;
    flat float material_idx;
    flat float ent_id;
} vs_out;

void main() {
    vec3 T = normalize(mat3(a_transform) * a_tangent);
    vec3 B = normalize(mat3(a_transform) * a_bitangent);
    vec3 N = normalize(mat3(a_transform) * a_normal);
    mat3 TBN = mat3(T, B, N);

    vs_out.world_space_position = (a_transform * vec4(a_pos, 1.0)).xyz;
    vs_out.view_space_position
        = (u_camera.view * vec4(vs_out.world_space_position, 1.0f)).xyz;
    vs_out.eye_position = u_camera.position.xyz;
    vs_out.TBN = TBN;
    vs_out.texture_uv = a_texture_uv;
    vs_out.material_idx = a_material_idx;
    vs_out.ent_id = a_ent_id;

    gl_Position = u_camera.view_projection * a_transform * vec4(a_pos, 1.0);
}
