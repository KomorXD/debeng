#version 430 core

out vec4 out_color;

in vec2 o_texture_uv;

uniform sampler2D u_screen_texture;

void main() {
    out_color = vec4(texture(u_screen_texture, o_texture_uv).rgb, 1.0);
}
