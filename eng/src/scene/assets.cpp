#include "eng/scene/assets.hpp"
#include "eng/renderer/primitives.hpp"
#include "eng/renderer/renderer.hpp"

namespace eng {

Mesh create_mesh(VertexData vertex_data) {
    Mesh mesh{};
    auto &[vertices, indices] = vertex_data;

    mesh.vao = VertexArray::create();
    mesh.vao.bind();

    VertexBuffer vbo = VertexBuffer::create();
    vbo.allocate(vertices.data(), vertices.size() * sizeof(Vertex),
                 vertices.size());

    VertexBufferLayout layout;
    layout.push_float(3); // 0 - position
    layout.push_float(3); // 1 - normal
    layout.push_float(3); // 2 - tangent
    layout.push_float(3); // 3 - bitangent
    layout.push_float(2); // 4 - texture uv

    IndexBuffer ibo = IndexBuffer::create();
    ibo.allocate(indices.data(), indices.size());
    mesh.vao.add_buffers(vbo, ibo, layout);

    layout.clear();
    layout.push_float(4); // 5 - transform
    layout.push_float(4); // 6 - transform
    layout.push_float(4); // 7 - transform
    layout.push_float(4); // 8 - transform
    layout.push_float(1); // 9 - entity id
    layout.push_float(1); // 10 - draw params idx

    vbo = VertexBuffer::create();
    vbo.allocate(nullptr, renderer::MAX_MESH_INSTANCES * sizeof(MeshInstance));
    mesh.vao.add_instanced_vertex_buffer(vbo, layout, 5);
    mesh.vao.unbind();

    return mesh;
}

AssetPack AssetPack::create(const std::string &pack_name) {
    AssetPack pack{};
    pack.name = pack_name;

    {
        Mesh quad_mesh = create_mesh(quad_vertex_data());
        quad_mesh.name = "Quad";
        (void)pack.add_mesh(quad_mesh);
    }

    {
        Mesh cube_mesh = create_mesh(cube_vertex_data());
        cube_mesh.name = "Cube";
        (void)pack.add_mesh(cube_mesh);
    }

    {
        Mesh sphere_mesh = create_mesh(uv_sphere_vertex_data());
        sphere_mesh.name = "Sphere";
        (void)pack.add_mesh(sphere_mesh);
    }

    {
        uint8_t white_pixel[] = { 255, 255, 255, 255 };
        Texture default_albedo =
            Texture::create(white_pixel, 1, 1, TextureFormat::RGBA8);
        default_albedo.name = "White";
        (void)pack.add_texture(default_albedo);
    }

    {
        uint8_t normal_pixel[] = { 127, 127, 255 };
        Texture default_normal =
            Texture::create(normal_pixel, 1, 1, TextureFormat::RGB8);
        default_normal.name = "Normal";
        (void)pack.add_texture(default_normal);
    }

    {
        Material def_mat;
        def_mat.name = "Base material";
        def_mat.shader_id = DEFAULT_BASE_MATERIAL;
        (void)pack.add_material(def_mat);
    }

    {
        Material def_mat;
        def_mat.name = "Flat material";
        def_mat.shader_id = DEFAULT_FLAT_MATERIAL;
        (void)pack.add_material(def_mat);
    }

    {
        Shader base_shader = Shader::create();
        base_shader.name = "Base";

        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/base.vert";
        spec.vertex_shader.replacements = {
            {
                "${CAMERA_BINDING}",
                std::to_string(renderer::CAMERA_BINDING)
            }
        };
        spec.fragment_shader.path = "resources/shaders/base.frag";
        spec.fragment_shader.replacements = {
            {
                "${CAMERA_BINDING}",
                std::to_string(renderer::CAMERA_BINDING)
            },
            {
                "${DIR_LIGHTS_BINDING}",
                std::to_string(renderer::DIR_LIGHTS_BINDING)
            },
            {
                "${MAX_DIR_LIGHTS}",
                std::to_string(renderer::MAX_DIR_LIGHTS)
            },
            {
                "${CASCADES_COUNT}",
                std::to_string(renderer::CASCADES_COUNT)
            },
            {
                "${POINT_LIGHTS_BINDING}",
                std::to_string(renderer::POINT_LIGHTS_BINDING)
            },
            {
                "${MAX_POINT_LIGHTS}",
                std::to_string(renderer::MAX_POINT_LIGHTS)
            },
            {
                "${SPOT_LIGHTS_BINDING}",
                std::to_string(renderer::SPOT_LIGHTS_BINDING)
            },
            {
                "${MAX_SPOT_LIGHTS}",
                std::to_string(renderer::MAX_SPOT_LIGHTS)
            },
            {
                "${SOFT_SHADOW_PROPS_BINDING}",
                std::to_string(renderer::SOFT_SHADOW_PROPS_BINDING)
            },
            {
                "${MAX_TEXTURES}",
                std::to_string(renderer::MAX_TEXTURES)
            },
            {
                "${DRAW_PARAMS_BINDING}",
                std::to_string(renderer::DRAW_PARAMS_BINDING)
            },
            {
                "${MAX_DRAW_PARAMS}",
                std::to_string(renderer::MAX_DRAW_PARAMS)
            }
        };

        assert(base_shader.build(spec) && "Default shaders not found");

        renderer::TextureSlots slots = eng::renderer::texture_slots();
        std::array<const char *, 9> unis = {"u_albedo",
                                            "u_normal",
                                            "u_roughness",
                                            "u_metallic",
                                            "u_ao",
                                            "u_dir_lights_csm_shadowmaps",
                                            "u_point_lights_shadowmaps",
                                            "u_spot_lights_shadowmaps",
                                            "u_soft_shadow_offsets_texture"};
        std::array<int32_t, 9> tex_slots = {slots.albedo,
                                            slots.normal,
                                            slots.roughness,
                                            slots.metallic,
                                            slots.ao,
                                            slots.dir_csm_shadowmaps,
                                            slots.point_lights_shadowmaps,
                                            slots.spot_lights_shadowmaps,
                                            slots.random_offsets_texture};

        base_shader.bind();
        for (int32_t i = 0; i < unis.size(); i++)
            base_shader.set_uniform_1i(unis[i], tex_slots[i]);

        (void)pack.add_shader(base_shader);
    }

    {
        Shader flat_shader = Shader::create();
        flat_shader.name = "Flat";

        ShaderSpec spec;
        spec.vertex_shader.path = "resources/shaders/flat.vert";
        spec.vertex_shader.replacements = {
            {
                "${CAMERA_BINDING}",
                std::to_string(renderer::CAMERA_BINDING)
            }
        };
        spec.fragment_shader.path = "resources/shaders/flat.frag";
        spec.fragment_shader.replacements = {
            {
                "${MAX_TEXTURES}",
                std::to_string(renderer::MAX_TEXTURES)
            },
            {
                "${DRAW_PARAMS_BINDING}",
                std::to_string(renderer::DRAW_PARAMS_BINDING)
            },
            {
                "${MAX_DRAW_PARAMS}",
                std::to_string(renderer::MAX_DRAW_PARAMS)
            }
        };

        assert(flat_shader.build(spec) && "Default shaders not found");

        flat_shader.bind();
        flat_shader.set_uniform_1i("u_albedo",
                                   renderer::texture_slots().albedo);

        (void)pack.add_shader(flat_shader);
    }

    return pack;
}

void AssetPack::destroy() {
    for (auto &[mesh_id, mesh] : meshes)
        mesh.vao.destroy();

    for (auto &[tex_id, texture] : textures)
        texture.destroy();

    for (auto &[env_map_id, env_map] : env_maps) {
        env_map.thumbnail.destroy();
        env_map.cube_map.destroy();
    }

    for (auto &[shader_id, shader] : shaders)
        shader.destroy();

    meshes.clear();
}

AssetID AssetPack::add_mesh(Mesh mesh) {
    AssetID id = 0;
    if (meshes.empty())
        id = 1;
    else
        id = meshes.rbegin()->first + 1;

    Mesh &new_mesh = meshes[id];
    new_mesh = mesh;
    return id;
}

AssetID AssetPack::add_texture(Texture &texture) {
    AssetID id = 0;
    if (textures.empty())
        id = 1;
    else
        id = textures.rbegin()->first + 1;

    Texture &new_texture = textures[id];
    new_texture = texture;
    return id;
}

AssetID AssetPack::add_env_map(EnvMap &env_map) {
    AssetID id = 0;
    if (env_maps.empty())
        id = 1;
    else
        id = env_maps.rbegin()->first + 1;

    EnvMap &new_env_map = env_maps[id];
    new_env_map.thumbnail = env_map.thumbnail;
    new_env_map.cube_map = env_map.cube_map;
    return id;
}

AssetID AssetPack::add_material(Material &material) {
    AssetID id = 0;
    if (materials.empty())
        id = 1;
    else
        id = materials.rbegin()->first + 1;

    Material &new_material = materials[id];
    new_material = material;
    return id;
}

AssetID AssetPack::add_shader(Shader &shader) {
    AssetID id = 0;
    if (shaders.empty())
        id = 1;
    else
        id = shaders.rbegin()->first + 1;

    Shader &new_shader = shaders[id];
    new_shader = shader;
    return id;
}

} // namespace eng
