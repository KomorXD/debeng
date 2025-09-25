#version 430 core

#define MATERIALS_BINDING ${MATERIALS_BINDING}
#define MAX_MATERIALS ${MAX_MATERIALS}

#define TEX_RECORDS_BINDING ${TEX_RECORDS_BINDING}
#define MAX_TEX_RECORDS ${MAX_TEX_RECORDS}

in VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    vec3 normal;
    vec2 texture_uv;
    flat float material_idx;
    flat float ent_id;
    flat float color_sens;
} fs_in;

layout(location = 0) out vec4 final_color;
layout(location = 1) out vec4 picker_id;

struct Material {
    vec4 color;
    vec2 tiling_factor;
    vec2 texture_offset;

    int albedo_record_idx;
    int normal_record_idx;
    int roughness_record_idx;
    int metallic_record_idx;
    int ao_record_idx;

    float roughness;
    float metallic;
    float ao;
};

layout (std140, binding = MATERIALS_BINDING) uniform Materials {
    Material materials[MAX_MATERIALS];
} u_materials;

struct TexRecord {
    vec2 offset;
    vec2 size;
    int layer;
    int record_id;

    vec2 padding;
};

layout (std140, binding = TEX_RECORDS_BINDING) uniform TexRecords {
    TexRecord tex_records[MAX_TEX_RECORDS];
} u_tex_records;

uniform sampler2D u_rgba_atlas;
uniform sampler2D u_rgb_atlas;
uniform sampler2D u_r_atlas;

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

    vec2 base_coords = fs_in.texture_uv * mat.tiling_factor + mat.texture_offset;
    base_coords -= floor(base_coords);

    TexRecord tex_record = u_tex_records.tex_records[mat.albedo_record_idx];
    vec2 tex_coords = base_coords * tex_record.size + tex_record.offset;
    vec4 albedo = texture(u_rgba_atlas, tex_coords) * mat.color;

    final_color = albedo;
    final_color.rgb *= fs_in.color_sens;
}
