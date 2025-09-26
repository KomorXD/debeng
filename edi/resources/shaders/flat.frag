#version 430 core

#define MATERIALS_BINDING ${MATERIALS_BINDING}
#define MAX_MATERIALS ${MAX_MATERIALS}

#define MAX_TEXTURES ${MAX_TEXTURES}

#define DRAW_PARAMS_BINDING ${DRAW_PARAMS_BINDING}
#define MAX_DRAW_PARAMS ${MAX_DRAW_PARAMS}

in VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    vec3 normal;
    vec2 texture_uv;
    flat float material_idx;
    flat float ent_id;
    flat float draw_params_idx;
} fs_in;

layout(location = 0) out vec4 final_color;
layout(location = 1) out vec4 picker_id;

struct Material {
    vec4 color;
    vec2 tiling_factor;
    vec2 texture_offset;

    int albedo_idx;
    int normal_idx;

    int roughness_idx;
    float roughness;

    int metallic_idx;
    float metallic;

    int ao_index;
    float ao;
};

layout (std140, binding = MATERIALS_BINDING) uniform Materials {
    Material materials[MAX_MATERIALS];
} u_materials;

struct DrawParams {
    float color_intensity;

    float pad1;
    float pad2;
    float pad3;
};

layout (std140, binding = DRAW_PARAMS_BINDING) uniform DrawParamsUni {
    DrawParams params[MAX_DRAW_PARAMS];
} u_draw_params;

uniform sampler2D u_textures[MAX_TEXTURES];
uniform sampler2DArrayShadow u_point_lights_shadowmaps;

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

    DrawParams params = u_draw_params.params[int(fs_in.draw_params_idx)];
    final_color = albedo;
    final_color.rgb *= max(1.0, params.color_intensity);
}
