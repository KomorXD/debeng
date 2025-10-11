#version 460 core

in vec3 o_world_pos;
in vec3 o_light_pos;
in float o_far_plane;

void main() {
    float dist = length(o_world_pos - o_light_pos);
    gl_FragDepth = dist / o_far_plane;
}
