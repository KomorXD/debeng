#version 430 core

layout (location = 0) in vec3 a_pos;
layout (location = 5) in mat4 a_transform;

void main() {
    gl_Position = a_transform * vec4(a_pos, 1.0);
}
