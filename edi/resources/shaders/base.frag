#version 430 core

#define CAMERA_BINDING ${CAMERA_BINDING}

#define DIR_LIGHTS_BINDING ${DIR_LIGHTS_BINDING}
#define MAX_DIR_LIGHTS ${MAX_DIR_LIGHTS}
#define CASCADES_COUNT ${CASCADES_COUNT}

#define POINT_LIGHTS_BINDING ${POINT_LIGHTS_BINDING}
#define MAX_POINT_LIGHTS ${MAX_POINT_LIGHTS}

#define SPOT_LIGHTS_BINDING ${SPOT_LIGHTS_BINDING}
#define MAX_SPOT_LIGHTS ${MAX_SPOT_LIGHTS}

#define SOFT_SHADOW_PROPS_BINDING ${SOFT_SHADOW_PROPS_BINDING}

#define DRAW_PARAMS_BINDING ${DRAW_PARAMS_BINDING}
#define MAX_DRAW_PARAMS ${MAX_DRAW_PARAMS}

in VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    mat3 TBN;
    vec3 tangent_world_position;
    vec3 tangent_view_position;
    vec2 texture_uv;
    flat float ent_id;
    flat float draw_params_idx;
} fs_in;

layout(location = 0) out vec4 final_color;
layout(location = 1) out vec4 picker_id;

struct DirLight {
    mat4 cascades_mats[CASCADES_COUNT];
    vec4 direction;
    vec4 color;
};

layout (std140, binding = DIR_LIGHTS_BINDING) uniform DirLights {
    DirLight lights[MAX_DIR_LIGHTS];
    int count;
} u_dir_lights;

struct PointLight {
    mat4 light_space_mats[6];
    vec4 position_and_linear;
    vec4 color_and_quadratic;
};

layout (std140, binding = POINT_LIGHTS_BINDING) uniform PointLights {
    PointLight lights[MAX_POINT_LIGHTS];
    int count;
} u_point_lights;

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

struct SoftShadowProps {
    int offsets_texture_size;
    int offsets_filter_size;
    float offset_radius;
};

layout (std140, binding = SOFT_SHADOW_PROPS_BINDING) uniform SoftShadowPropsUni {
    SoftShadowProps props;
} u_soft_shadow_props;

struct DrawParams {
    float color_intensity;

    float pad1;
    float pad2;
    float pad3;
};

layout (std140, binding = DRAW_PARAMS_BINDING) uniform DrawParamsUni {
    DrawParams params[MAX_DRAW_PARAMS];
} u_draw_params;

layout (std140, binding = CAMERA_BINDING) uniform Camera {
    mat4 view_projection;
    mat4 projection;
    mat4 view;
    vec4 position;
    vec2 viewport;

    float exposure;
    float gamma;
    float near_clip;
    float far_clip;
    float fov;

    float bloom_strength;
    float bloom_threshold;
} u_camera;

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
uniform sampler2D u_normal;
uniform sampler2D u_roughness;
uniform sampler2D u_metallic;
uniform sampler2D u_ao;

uniform float u_cascade_distances[CASCADES_COUNT];
uniform sampler2DArrayShadow u_dir_lights_csm_shadowmaps;
uniform sampler2DArrayShadow u_point_lights_shadowmaps;
uniform sampler2DArrayShadow u_spot_lights_shadowmaps;
uniform sampler3D u_soft_shadow_offsets_texture;

float calc_shadow(sampler2DArrayShadow shadowmaps, mat4 light_space_mat,
                  int layer, vec3 N, vec3 L) {
    vec4 frag_position_light_space
        = light_space_mat * vec4(fs_in.world_space_position, 1.0);
    vec3 proj_coords = frag_position_light_space.xyz
        / frag_position_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    float current_depth = proj_coords.z;
    if (current_depth > 1.0)
        return 0.0;

    SoftShadowProps props = u_soft_shadow_props.props;
    vec2 f
        = mod(gl_FragCoord.xy, vec2(props.offsets_texture_size));
    ivec3 offset_coord;
    offset_coord.yz = ivec2(f);

    int samples_div2 = int(props.offsets_filter_size
                           * props.offsets_filter_size / 2.0);
    vec4 sc = vec4(proj_coords, 1.0);
    const vec2 texel_size = 1.0 / textureSize(shadowmaps, 0).xy;

    float depth = 0.0;
    float shadow = 0.0;
    for (int i = 0; i < 4; i++) {
        offset_coord.x = i;
        vec4 offsets = texelFetch(u_soft_shadow_offsets_texture,
                                  offset_coord, 0) * props.offset_radius;

        sc.xy = proj_coords.xy + offsets.rg * texel_size;
        depth = texture(shadowmaps, vec4(sc.xy, layer, proj_coords.z));
        shadow += depth;

        sc.xy = proj_coords.xy + offsets.ba * texel_size;
        depth = texture(shadowmaps, vec4(sc.xy, layer, proj_coords.z));
        shadow += depth;
    }

    shadow /= 8.0;
    if (shadow == 0.0 || shadow == 1.0)
        return shadow;

    shadow *= 8.0;
    for (int i = 4; i < samples_div2; i++) {
        offset_coord.x = i;
        vec4 offsets = texelFetch(u_soft_shadow_offsets_texture,
                                  offset_coord, 0) * props.offset_radius;

        sc.xy = proj_coords.xy + offsets.rg * texel_size;
        depth = texture(shadowmaps, vec4(sc.xy, layer, proj_coords.z));
        shadow += depth;

        sc.xy = proj_coords.xy + offsets.ba * texel_size;
        depth = texture(shadowmaps, vec4(sc.xy, layer, proj_coords.z));
        shadow += depth;
    }

    shadow /= float(samples_div2) * 2.0;
    return shadow;
}

float calc_csm_shadow(int dir_light_idx, vec3 N, vec3 L) {
    vec4 frag_pos_view_space
        = u_camera.view * vec4(fs_in.world_space_position, 1.0);
    float depth = abs(frag_pos_view_space.z);

    int layer = -1;
    for (int i = 0; i < CASCADES_COUNT; i++) {
        if (depth < u_cascade_distances[i]) {
            layer = i;
            break;
        }
    }

    if (layer == -1)
        return 1.0;

    return calc_shadow(u_dir_lights_csm_shadowmaps,
                       u_dir_lights.lights[dir_light_idx].cascades_mats[layer],
                       dir_light_idx * CASCADES_COUNT + layer, N, L);
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

vec3 fresnelSchlickRoughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 berley_brdf(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    float energy_bias = roughness;
    float energy_factor = mix(1.0, 1.0 / (4.0 * NdotV + 2.0), energy_bias);
    F *= energy_factor;

    float D = dist_ggx(N, H, roughness);
    float G = geo_smith(N, V, L, roughness);

    float denom = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = D * G * F / denom;
    float clamp_scale = mix(0.0, 1.0, clamp(1.0 - roughness, 0.0, 1.0));
    specular = specular / (1.0 + specular * clamp_scale);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float FD90 = 0.5 + 2.0 * LdotH * LdotH * roughness;
    float light_scatter = 1.0 + (FD90 - 1.0) * pow(1.0 - NdotL, 5.0);
    float view_scatter = 1.0 + (FD90 - 1.0) * pow(1.0 - NdotV, 5.0);
    vec3 diffuse_term = albedo * (light_scatter * view_scatter) * (1.0 / PI) * NdotL;

    return kD * diffuse_term + specular;
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

    vec2 tex_coords
        = fs_in.texture_uv * u_material.tiling_factor + u_material.texture_offset;

    vec4 diffuse = texture(u_albedo, tex_coords);
    vec3 N = texture(u_normal, tex_coords).rgb;
    N = N * 2.0 - 1.0;

    float roughness = texture(u_roughness, tex_coords).r * u_material.roughness;
    float metallic = texture(u_metallic, tex_coords).r * u_material.metallic;
    float ao = texture(u_ao, tex_coords).r * u_material.ao;

    vec3 V = normalize(fs_in.tangent_view_position - fs_in.tangent_world_position);

    vec3 Lo = vec3(0.0);
    vec3 F0 = mix(vec3(0.04), diffuse.rgb, metallic);
    for (int i = 0; i < u_dir_lights.count; i++) {
        DirLight dl = u_dir_lights.lights[i];

        vec3 radiance = dl.color.rgb;
        vec3 L = fs_in.TBN * dl.direction.xyz;
        vec3 brdf = berley_brdf(N, V, L, diffuse.rgb, metallic, roughness);

        float shadow = calc_csm_shadow(i, N, L);
        Lo += brdf * radiance * shadow * ao;
    }

    for (int i = 0; i < u_point_lights.count; i++) {
        PointLight pl           = u_point_lights.lights[i];
        vec3 position           = pl.position_and_linear.xyz;
        vec3 tangent_position   = fs_in.TBN * position;
        vec3 color              = pl.color_and_quadratic.rgb;
        float linear            = pl.position_and_linear.w;
        float quadratic         = pl.color_and_quadratic.w;

        float dist = length(position - fs_in.world_space_position);
        float attentuation
            = 1.0 / (1.0 + linear * dist + quadratic * dist * dist);

        vec3 radiance = color * attentuation;
        vec3 L = normalize(tangent_position - fs_in.tangent_world_position);
        vec3 brdf = berley_brdf(N, V, L, diffuse.rgb, metallic, roughness);

        vec3 dir = fs_in.world_space_position - position;
        vec3 dirs[] = {
            vec3(1.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
            vec3(0.0, -1.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, -1.0)
        };
        int dir_idx = -1;
        float max_dot = 0.0;
        for (int i = 0; i < 6; i++) {
            float d = dot(dirs[i], dir);
            if (d > max_dot) {
                dir_idx = i;
                max_dot = d;
            }
        }

        float shadow = calc_shadow(u_point_lights_shadowmaps,
                                   pl.light_space_mats[dir_idx],
                                   i * 6 + dir_idx, N, L);
        Lo += brdf * radiance * shadow * ao;
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

            float dist = length(position - fs_in.world_space_position);
            float attentuation
                = 1.0 / (1.0 + linear * dist + quadratic * dist * dist);

            vec3 radiance = color * attentuation;
            vec3 brdf = berley_brdf(N, V, L, diffuse.rgb, metallic, roughness);

            float shadow
                = calc_shadow(u_spot_lights_shadowmaps, sl.light_space_mat, i,
                              N, L);
            Lo += brdf * radiance * shadow * ao;
        }
    }

    final_color.rgb = (0.1 + Lo) * u_material.color.rgb;

    DrawParams params = u_draw_params.params[int(fs_in.draw_params_idx)];
    final_color.rgb *= max(1.0, params.color_intensity);
    final_color.a = diffuse.a * u_material.color.a;
}
