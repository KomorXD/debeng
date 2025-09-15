#version 430 core

#define POINT_LIGHTS_BINDING ${POINT_LIGHTS_BINDING}
#define MAX_POINT_LIGHTS ${MAX_POINT_LIGHTS}

#define MATERIALS_BINDING ${MATERIALS_BINDING}
#define MAX_MATERIALS ${MAX_MATERIALS}

#define MAX_TEXTURES ${MAX_TEXTURES}

in VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    vec3 normal;
    vec2 texture_uv;
    flat float material_idx;
    flat float ent_id;
} fs_in;

layout(location = 0) out vec4 final_color;
layout(location = 1) out vec4 picker_id;

struct PointLight {
    vec4 position_and_linear;
    vec4 color_and_quadratic;
};

layout (std140, binding = POINT_LIGHTS_BINDING) uniform PointLights {
    PointLight lights[MAX_POINT_LIGHTS];
    int count;
} u_points_lights;

struct Material {
    vec4 color;
    vec2 tiling_factor;
    vec2 texture_offset;

    int albedo_idx;
};

layout (std140, binding = MATERIALS_BINDING) uniform Materials {
    Material materials[MAX_MATERIALS];
} u_materials;

uniform sampler2D u_textures[MAX_TEXTURES];

vec3 diffuse_impact(PointLight light) {
    vec3 pos = light.position_and_linear.xyz;
    vec3 color = light.color_and_quadratic.xyz;
    float linear = light.position_and_linear.w;
    float quadratic = light.color_and_quadratic.w;

    vec3 light_dir = normalize(pos - fs_in.world_space_position);
    float strength = max(dot(fs_in.normal, light_dir), 0.0);

    return strength * color;
}

vec3 specular_impact(PointLight light) {
    vec3 pos = light.position_and_linear.xyz;
    vec3 color = light.color_and_quadratic.xyz;

    vec3 view_dir = normalize(fs_in.eye_position - fs_in.world_space_position);
    vec3 light_dir = normalize(pos - fs_in.world_space_position);
    vec3 halfway_dir = normalize(light_dir + view_dir);

    float strength = pow(max(dot(fs_in.normal, halfway_dir), 0.0f), 32.0);

    return strength * color;
}

void main() {
    int ent_id = int(fs_in.ent_id);
    int r_int = int(mod(int(ent_id / 65025.0), 255));
    int g_int = int(mod(int(ent_id / 255.0), 255));
    int b_int = int(mod(ent_id, 255));
    float r = float(r_int) / 255.0;
    float g = float(g_int) / 255.0;
    float b = float(b_int) / 255.0;
    picker_id = vec4(r, g, b, 1.0);

    Material mat = u_materials.materials[int(fs_in.material_idx)];
    vec2 tex_coords
        = fs_in.texture_uv * mat.tiling_factor + mat.texture_offset;
    vec4 albedo = texture(u_textures[mat.albedo_idx], tex_coords) * mat.color;

    vec3 ambient = vec3(0.1);
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < u_points_lights.count; i++) {
        PointLight light = u_points_lights.lights[i];
        float linear = light.position_and_linear.w;
        float quadratic = light.color_and_quadratic.w;

        float dist
            = length(light.position_and_linear.xyz - fs_in.world_space_position);
        float attentuation
            = 1.0 / (1.0 + linear * dist + quadratic * dist * dist);

        ambient += vec3(0.1) * attentuation;
        diffuse += diffuse_impact(light) * attentuation;
        specular += specular_impact(light) * attentuation;
    }

    final_color = vec4(ambient + diffuse + specular, 1.0) * albedo;
}
