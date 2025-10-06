#version 430 core

#define SPOT_LIGHTS_BINDING ${SPOT_LIGHTS_BINDING}
#define MAX_SPOT_LIGHTS ${MAX_SPOT_LIGHTS}

layout(triangles, invocations = ${INVOCATIONS}) in;
layout(triangle_strip, max_vertices = 3) out;

struct SpotLight {
    mat4 light_space_mat;
    vec4 pos_and_cutoff;
    vec4 dir_and_outer_cutoff;
    vec4 color_and_linear;
    float quadratic;
};

layout (std430, binding = SPOT_LIGHTS_BINDING) buffer SpotLights {
    int count;
    SpotLight lights[];
} u_spot_lights;

void main()
{
    if(gl_InvocationID >= u_spot_lights.count)
        return;

    for(int v = 0; v < 3; v++) {
        gl_Layer = gl_InvocationID;
        gl_Position = u_spot_lights.lights[gl_InvocationID].light_space_mat
            * gl_in[v].gl_Position;

        EmitVertex();
    }

    EndPrimitive();
}
