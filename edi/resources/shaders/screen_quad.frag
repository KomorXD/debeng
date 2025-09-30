#version 430 core

#define CAMERA_BINDING ${CAMERA_BINDING}

out vec4 out_color;

in vec2 o_texture_uv;

uniform sampler2D u_screen_texture;
uniform sampler2D u_bloom_texture;

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
    float bloom_radius;
} u_camera;

void main() {
    vec4 color = texture(u_screen_texture, o_texture_uv);
    vec3 bloom = texture(u_bloom_texture, o_texture_uv).rgb;

    color.rgb += bloom * u_camera.bloom_strength;
    vec3 mapped = vec3(1.0) - exp(-color.rgb * u_camera.exposure);
    mapped = pow(mapped, vec3(1.0 / u_camera.gamma));

    out_color = vec4(mapped, color.a);
}
