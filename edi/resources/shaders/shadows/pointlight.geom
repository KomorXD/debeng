#version 430 core

#define POINT_LIGHTS_BINDING ${POINT_LIGHTS_BINDING}

layout (triangles, invocations = ${INVOCATIONS}) in;
layout (triangle_strip, max_vertices = 18) out;

struct PointLight {
    vec4 position_and_radius;
    vec3 color;
};

layout (std430, binding = POINT_LIGHTS_BINDING) buffer PointLights {
    int count;
    PointLight lights[];
} u_point_lights;

mat4 perspective_90deg(float near, float far) {
    float f = 1.0 / tan(radians(90.0) / 2.0);

    return mat4(
        f,   0.0, 0.0, 0.0,
        0.0, f,   0.0, 0.0,
        0.0, 0.0, (far + near) / (near - far), -1.0,
        0.0, 0.0, (2.0 * far * near) / (near - far), 0.0
    );
}

mat4 look_at(vec3 origin, vec3 target, vec3 up) {
    vec3 z = normalize(origin - target);
    vec3 x = normalize(cross(up, z));
    vec3 y = cross(z, x);

    return mat4(
        x.x, y.x, z.x, 0.0,
        x.y, y.y, z.y, 0.0,
        x.z, y.z, z.z, 0.0,
        -dot(x, origin), -dot(y, origin), -dot(z, origin), 1.0
    );
}

uniform int u_offset;

out vec3 o_world_pos;
out vec3 o_light_pos;
out float o_far_plane;

void main() {
    if (gl_InvocationID >= u_point_lights.count)
        return;

    int iid = gl_InvocationID + u_offset;
    vec3 light_pos = u_point_lights.lights[iid].position_and_radius.xyz;
    float radius = u_point_lights.lights[iid].position_and_radius.w;

    const vec3 face_dirs[6] = {
        vec3(1.0, 0.0, 0.0), vec3(-1.0,  0.0,  0.0),
        vec3(0.0, 1.0, 0.0), vec3( 0.0, -1.0,  0.0),
        vec3(0.0, 0.0, 1.0), vec3( 0.0,  0.0, -1.0)
    };

    const vec3 face_ups[6] = {
        vec3(0.0, -1.0, 0.0), vec3(0.0, -1.0,  0.0),
        vec3(0.0,  0.0, 1.0), vec3(0.0,  0.0, -1.0),
        vec3(0.0, -1.0, 0.0), vec3(0.0, -1.0,  0.0)
    };

    mat4 proj = perspective_90deg(0.1, radius);
    int base_layer = iid * 6;

    for (int face = 0; face < 6; face++) {
        vec3 dir = face_dirs[face];
        vec3 up = face_ups[face];

        mat4 view = look_at(light_pos, light_pos + dir, up);
        mat4 view_proj = proj * view;

        gl_Layer = base_layer + face;
        for (int v = 0; v < 3; v++) {
            o_world_pos = gl_in[v].gl_Position.xyz;
            gl_Position = view_proj * vec4(o_world_pos, 1.0);

            o_light_pos = light_pos;
            o_far_plane = radius;

            EmitVertex();
        }

        EndPrimitive();
    }
}
