#version 430 core

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texture_uv;

out vec2 o_texture_uv;

void main() {
    o_texture_uv = a_texture_uv;
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
