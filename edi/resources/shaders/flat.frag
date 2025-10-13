#version 430 core

in VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    vec3 normal;
    vec2 texture_uv;
    flat float ent_id;
} fs_in;

layout(location = 0) out vec4 final_color;
layout(location = 1) out vec4 picker_id;

struct Material {
    vec4 color;
    vec2 tiling_factor;
    vec2 texture_offset;

    float roughness;
    float metallic;
    float ao;

    float padding;
};

uniform Material u_material;
uniform sampler2D u_albedo;

void main() {
    int ent_id = int(fs_in.ent_id);
    int r_int = int(mod(int(ent_id / 65025.0), 255));
    int g_int = int(mod(int(ent_id / 255.0), 255));
    int b_int = int(mod(ent_id, 255));
    float r = float(r_int) / 255.0;
    float g = float(g_int) / 255.0;
    float b = float(b_int) / 255.0;
    picker_id = vec4(r, g, b, 1.0);

    vec2 tex_coords
        = fs_in.texture_uv * u_material.tiling_factor + u_material.texture_offset;
    vec4 albedo = texture(u_albedo, tex_coords) * u_material.color;

    final_color = albedo;
}
