#version 430 core

#define POINT_LIGHTS_BINDING ${POINT_LIGHTS_BINDING}
#define MAX_POINT_LIGHTS ${MAX_POINT_LIGHTS}

layout (triangles, invocations = ${INVOCATIONS}) in;
layout (triangle_strip, max_vertices = 18) out;

struct PointLight {
    mat4 light_space_mats[6];
    vec4 position_and_linear;
    vec4 color_and_quadratic;
};

layout (std430, binding = POINT_LIGHTS_BINDING) buffer PointLights {
    int count;
    PointLight lights[];
} u_point_lights;

void main() {
    if (gl_InvocationID >= u_point_lights.count)
        return;

    int layer = gl_InvocationID * 6;
    for (int face = 0; face < 6; face++) {
        for (int v = 0; v < 3; v++) {
            gl_Layer = layer;
            gl_Position
                = u_point_lights.lights[gl_InvocationID].light_space_mats[face]
                * gl_in[v].gl_Position;

            EmitVertex();
        }

        layer++;
        EndPrimitive();
    }
}
