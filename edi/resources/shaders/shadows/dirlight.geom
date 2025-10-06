#version 430 core

#define DIR_LIGHTS_BINDING ${DIR_LIGHTS_BINDING}
#define MAX_DIR_LIGHTS ${MAX_DIR_LIGHTS}
#define CASCADES_COUNT ${CASCADES_COUNT}

layout (triangles, invocations = ${INVOCATIONS}) in;
layout (triangle_strip, max_vertices = ${MAX_VERTICES}) out;

struct DirLight {
    mat4 cascades_mats[CASCADES_COUNT];
    vec4 direction;
    vec4 color;
};

layout (std430, binding = DIR_LIGHTS_BINDING) buffer DirLights {
    int count;
    DirLight lights[];
} u_dir_lights;

void main() {
    if (gl_InvocationID >= u_dir_lights.count)
        return;

    int layer = gl_InvocationID * CASCADES_COUNT;
    for (int cascade = 0; cascade < CASCADES_COUNT; cascade++) {
        for (int v = 0; v < 3; v++) {
            gl_Layer = layer;
            gl_Position
                = u_dir_lights.lights[gl_InvocationID].cascades_mats[cascade]
                * gl_in[v].gl_Position;

            EmitVertex();
        }

        layer++;
        EndPrimitive();
    }
}
