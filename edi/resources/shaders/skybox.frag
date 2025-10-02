#version 460 core

in vec3 local_pos;

out vec4 color;

uniform samplerCube u_cubemap;

void main() {
    color = vec4(texture(u_cubemap, local_pos).rgb, 1.0);
}
