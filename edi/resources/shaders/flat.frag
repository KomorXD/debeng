#version 430 core

#define DRAW_PARAMS_BINDING ${DRAW_PARAMS_BINDING}
#define MAX_DRAW_PARAMS ${MAX_DRAW_PARAMS}

in VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    vec3 normal;
    vec2 texture_uv;
    flat float ent_id;
    flat float draw_params_idx;
} fs_in;

layout(location = 0) out vec4 final_color;
layout(location = 1) out vec4 picker_id;

struct DrawParams {
    float color_intensity;

    float pad1;
    float pad2;
    float pad3;
};

layout (std140, binding = DRAW_PARAMS_BINDING) uniform DrawParamsUni {
    DrawParams params[MAX_DRAW_PARAMS];
} u_draw_params;

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

    DrawParams params = u_draw_params.params[int(fs_in.draw_params_idx)];
    final_color = albedo;
    final_color.rgb *= max(1.0, params.color_intensity);
}
