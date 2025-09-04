#version 430 core

in VS_OUT {
    vec3 world_space_position;
    vec3 view_space_position;
    vec3 eye_position;
    vec3 normal;
    vec2 texture_uv;
} fs_in;

out vec4 final_color;

uniform sampler2D u_texture;

struct PointLight {
    vec4 position_and_linear;
    vec4 color_and_quadratic;
};

layout (std140, binding = 1) uniform PointLights {
    PointLight lights[128];
    int count;
} u_points_lights;

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
    vec4 albedo = texture(u_texture, fs_in.texture_uv);

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
