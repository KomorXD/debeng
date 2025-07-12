#version 430 core

in vec2 tex_pos;
out vec4 final_color;

uniform sampler2D u_texture;

void main() {
    vec4 color = texture(u_texture, tex_pos);
    final_color = color;
}
