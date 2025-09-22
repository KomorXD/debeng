#version 430 core

#define POINT_LIGHTS_BINDING ${POINT_LIGHTS_BINDING}
#define MAX_POINT_LIGHTS ${MAX_POINT_LIGHTS}

#define DIR_LIGHTS_BINDING ${DIR_LIGHTS_BINDING}
#define MAX_DIR_LIGHTS ${MAX_DIR_LIGHTS}

#define SPOT_LIGHTS_BINDING ${SPOT_LIGHTS_BINDING}
#define MAX_SPOT_LIGHTS ${MAX_SPOT_LIGHTS}

#define MATERIALS_BINDING ${MATERIALS_BINDING}
#define MAX_MATERIALS ${MAX_MATERIALS}

#define MAX_TEXTURES ${MAX_TEXTURES}

in VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    mat3 TBN;
    vec3 tangent_world_position;
    vec3 tangent_view_position;
    vec2 texture_uv;
    flat float material_idx;
    flat float ent_id;
    flat float color_sens;
} fs_in;

layout(location = 0) out vec4 final_color;
layout(location = 1) out vec4 picker_id;

struct PointLight {
    mat4 light_space_mats[6];
    vec4 position_and_linear;
    vec4 color_and_quadratic;
};

layout (std140, binding = POINT_LIGHTS_BINDING) uniform PointLights {
    PointLight lights[MAX_POINT_LIGHTS];
    int count;
} u_point_lights;

struct DirLight {
    vec4 direction;
    vec4 color;
};

layout (std140, binding = DIR_LIGHTS_BINDING) uniform DirLights {
    DirLight lights[MAX_DIR_LIGHTS];
    int count;
} u_dir_lights;

struct SpotLight {
    mat4 light_space_mat;
    vec4 pos_and_cutoff;
    vec4 dir_and_outer_cutoff;
    vec4 color_and_linear;
    float quadratic;
};

layout (std140, binding = SPOT_LIGHTS_BINDING) uniform SpotLights {
    SpotLight lights[MAX_SPOT_LIGHTS];
    int count;
} u_spot_lights;

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

    int ao_idx;
    float ao;
};

layout (std140, binding = MATERIALS_BINDING) uniform Materials {
    Material materials[MAX_MATERIALS];
} u_materials;

uniform sampler2D u_textures[MAX_TEXTURES];
uniform sampler2DArrayShadow u_spot_lights_shadowmaps;
uniform sampler2DArrayShadow u_point_lights_shadowmaps;

float calc_shadow(sampler2DArrayShadow shadowmaps, mat4 light_space_mat,
                  int layer) {
    vec4 frag_position_light_space
        = light_space_mat * vec4(fs_in.world_space_position, 1.0);
    vec3 proj_coords = frag_position_light_space.xyz
        / frag_position_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    float depth = proj_coords.z;
    float shadow_depth
        = texture(shadowmaps, vec4(proj_coords.xy, layer, proj_coords.z));

    if (depth < shadow_depth)
        return 1.0;

    return 0.0;
}

const float PI = 3.14159265359;

float dist_ggx(vec3 N, vec3 H, float roughness) {
    float a     = roughness * roughness;
    float a2    = a * a;
    float NH    = max(dot(N, H), 0.0);
    float NH2   = NH * NH;
    float denom = (NH2 * (a2 - 1.0) + 1.0);
    denom       = PI * denom * denom;

    return a2 / denom;
}

float geo_schlick_ggx(float NV, float roughness) {
    float a     = roughness + 1.0;
    float k     = (a * a) / 8.0;
    float denom = NV * (1.0 - k) + k;

    return NV / denom;
}

float geo_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NV = max(dot(N, V), 0.0);
    float NL = max(dot(N, L), 0.0);

    float ggx2 = geo_schlick_ggx(NV, roughness);
    float ggx1 = geo_schlick_ggx(NL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
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

    vec4 diffuse = texture(u_textures[mat.albedo_idx], tex_coords);
    float roughness = texture(u_textures[mat.roughness_idx], tex_coords).r * mat.roughness;
    float metallic = texture(u_textures[mat.metallic_idx], tex_coords).r * mat.metallic;
    float ao = texture(u_textures[mat.ao_idx], tex_coords).r * mat.ao;
    vec3 N = texture(u_textures[mat.normal_idx], tex_coords).rgb;
    N = N * 2.0 - 1.0;

    vec3 V = normalize(fs_in.tangent_view_position - fs_in.tangent_world_position);

    vec3 Lo = vec3(0.0);
    vec3 F0 = mix(vec3(0.04), diffuse.rgb, metallic);
    for (int i = 0; i < u_point_lights.count; i++) {
        PointLight pl           = u_point_lights.lights[i];
        vec3 position           = pl.position_and_linear.xyz;
        vec3 tangent_position   = fs_in.TBN * position;
        vec3 color              = pl.color_and_quadratic.rgb;
        float linear            = pl.position_and_linear.w;
        float quadratic         = pl.color_and_quadratic.w;

        vec3 L = normalize(tangent_position - fs_in.tangent_world_position);
        vec3 H = normalize(V + L);

        float dist = length(position - fs_in.world_space_position);
        float attentuation
            = 1.0 / (1.0 + linear * dist + quadratic * dist * dist);

        vec3 radiance = color * attentuation;

        float NDF   = dist_ggx(N, H, roughness);
        float G     = geo_smith(N, V, L, roughness);
        vec3 F      = fresnel_schlick(clamp(dot(H, V), 0.0, 1.0), F0);

        float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = NDF * G * F / denom;
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float shadow = 0.0;
        int local_layer = i * 6;
        for (int face = 0; face < 6; face++) {
            shadow += calc_shadow(u_point_lights_shadowmaps,
                                  pl.light_space_mats[face], local_layer);
            local_layer++;
        }

        Lo += (kD * diffuse.rgb / PI + specular) * radiance
            * max(dot(N, L), 0.0) * shadow;
    }

    for (int i = 0; i < u_dir_lights.count; i++) {
        DirLight dl = u_dir_lights.lights[i];

        vec3 radiance = dl.color.rgb;
        vec3 L = fs_in.TBN * dl.direction.xyz;
        vec3 H = normalize(V + L);

        float NDF   = dist_ggx(N, H, roughness);
        float G     = geo_smith(N, V, L, roughness);
        vec3 F      = fresnel_schlick(clamp(dot(H, V), 0.0, 1.0), F0);

        float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = NDF * G * F / denom;
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo += (kD * diffuse.rgb / PI + specular) * radiance * max(dot(N, L), 0.0);
    }

    for (int i = 0; i < u_spot_lights.count; i++) {
        SpotLight sl = u_spot_lights.lights[i];

        vec3 position           = sl.pos_and_cutoff.xyz;
        vec3 tangent_position   = fs_in.TBN * position;
        vec3 direction          = fs_in.TBN * sl.dir_and_outer_cutoff.xyz;
        vec3 color              = sl.color_and_linear.rgb;

        float cutoff        = sl.pos_and_cutoff.w;
        float outer_cutoff  = sl.dir_and_outer_cutoff.w;
        float linear        = sl.color_and_linear.w;
        float quadratic     = sl.quadratic;

        vec3 L = normalize(tangent_position - fs_in.tangent_world_position);
        float theta = dot(L, normalize(-direction));

        if (theta > cutoff) {
            float epsilon = abs(cutoff - outer_cutoff) + 0.0001;
            float intensity = clamp((theta - outer_cutoff) / epsilon, 0.0, 1.0);

            vec3 H = normalize(V + L);
            float dist = length(position - fs_in.world_space_position);
            float attentuation
                = 1.0 / (1.0 + linear * dist + quadratic * dist * dist);
            vec3 radiance = color * attentuation;

            float NDF   = dist_ggx(N, H, roughness);
            float G     = geo_smith(N, V, L, roughness);
            vec3 F      = fresnel_schlick(clamp(dot(H, V), 0.0, 1.0), F0);

            float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
            vec3 specular = NDF * G * F / denom;
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            float shadow_factor
                = calc_shadow(u_spot_lights_shadowmaps, sl.light_space_mat, i);

            Lo += (kD * diffuse.rgb / PI + specular) * radiance
                * max(dot(N, L), 0.0) * intensity * shadow_factor;
        }
    }

    final_color.rgb = (0.1 + Lo) * mat.color.rgb * ao;
    final_color.rgb *= fs_in.color_sens;
    final_color.a = diffuse.a * mat.color.a;
}
