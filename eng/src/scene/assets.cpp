#include "eng/scene/assets.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/renderer/primitives.hpp"
#include "eng/renderer/renderer.hpp"
#include <filesystem>

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
    layout.push_float(1); // 9 - material idx
    layout.push_float(1); // 10 - entity idx
    layout.push_float(1); // 11 - color sens

    vbo = VertexBuffer::create();
    vbo.allocate(nullptr, 128 * sizeof(MeshInstance));
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
        AssetID ctxs[] = {ATLAS_RGBA8, ATLAS_RGB8, ATLAS_R8};
        TextureFormat formats[] = {TextureFormat::RGBA8, TextureFormat::RGB8,
                                   TextureFormat::R8};

        for (int32_t i = 0; i < 3; i++) {
            AtlasContext &ctx = pack.atlases[ctxs[i]];
            ctx.atlas = Texture::create(nullptr, 8192, 8192, formats[i]);
            ctx.nodes.resize(256);

            stbrp_init_target(&ctx.context, ctx.atlas.width, ctx.atlas.height,
                              ctx.nodes.data(), ctx.nodes.size());
        }
    }

    {
        uint8_t white_pixel[] = { 255, 255, 255, 255 };
        AssetID id = pack.add_texture(white_pixel, 1, 1, TextureFormat::RGBA8);
        pack.tex_records.at(id).file_name = "White";
    }

    {
        uint8_t normal_pixel[] = { 127, 127, 255 };
        AssetID id = pack.add_texture(normal_pixel, 1, 1, TextureFormat::RGB8);
        pack.tex_records.at(id).file_name = "Normal";
    }

    {
        uint8_t white = 255;
        AssetID id = pack.add_texture(&white, 1, 1, TextureFormat::R8);
        pack.tex_records.at(id).file_name = "White R";
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
                "${MATERIALS_BINDING}",
                std::to_string(renderer::MATERIALS_BINDING)
            },
            {
                "${MAX_MATERIALS}",
                std::to_string(renderer::MAX_MATERIALS)
            },
            {
                "${TEX_RECORDS_BINDING}",
                std::to_string(renderer::TEX_RECORDS_BINDING)
            },
            {
                "${MAX_TEX_RECORDS}",
                std::to_string(renderer::MAX_TEX_RECORDS)
            }
        };

        assert(base_shader.build(spec) && "Default shaders not found");

        renderer::TextureSlots slots = eng::renderer::texture_slots();
        base_shader.bind();
        base_shader.set_uniform_1i("u_rgba_atlas", slots.rgba_atlas);
        base_shader.set_uniform_1i("u_rgb_atlas", slots.rgb_atlas);
        base_shader.set_uniform_1i("u_r_atlas", slots.r_atlas);
        base_shader.set_uniform_1i("u_dir_lights_csm_shadowmaps",
                                   slots.dir_csm_shadowmaps);
        base_shader.set_uniform_1i("u_point_lights_shadowmaps",
                                   slots.point_lights_shadowmaps);
        base_shader.set_uniform_1i("u_spot_lights_shadowmaps",
                                   slots.spot_lights_shadowmaps);
        base_shader.set_uniform_1i("u_soft_shadow_offsets_texture",
                                   slots.random_offsets_texture);

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
                "${MATERIALS_BINDING}",
                std::to_string(renderer::MATERIALS_BINDING)
            },
            {
                "${MAX_MATERIALS}",
                std::to_string(renderer::MAX_MATERIALS)
            },
            {
                "${TEX_RECORDS_BINDING}",
                std::to_string(renderer::TEX_RECORDS_BINDING)
            },
            {
                "${MAX_TEX_RECORDS}",
                std::to_string(renderer::MAX_TEX_RECORDS)
            }
        };

        assert(flat_shader.build(spec) && "Default shaders not found");

        renderer::TextureSlots slots = eng::renderer::texture_slots();
        flat_shader.bind();
        flat_shader.set_uniform_1i("u_rgba_atlas", slots.rgba_atlas);

        (void)pack.add_shader(flat_shader);
    }

    return pack;
}

void AssetPack::destroy() {
    for (auto &[mesh_id, mesh] : meshes)
        mesh.vao.destroy();

    for (auto &[shader_id, shader] : shaders)
        shader.destroy();

    tex_records.clear();

    for (auto &[atlas_id, atlas_ctx] : atlases)
        atlas_ctx.atlas.destroy();

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

AssetID AssetPack::add_texture(const void *data, int32_t width, int32_t height,
                               TextureFormat format,
                               std::optional<std::string> path) {
    AssetID id = 0;
    if (tex_records.empty())
        id = 1;
    else
        id = tex_records.rbegin()->first + 1;

    AssetID owning_atlas = 1;

    switch (format) {
    case TextureFormat::RGBA8:
        owning_atlas = ATLAS_RGBA8;
        break;
    case TextureFormat::RGB8:
        owning_atlas = ATLAS_RGB8;
        break;
    case TextureFormat::R8:
        owning_atlas = ATLAS_R8;
        break;
    default:
        assert(true && "Unsupported texture asset type");
        break;
    }

    TextureRecord &new_record = tex_records[id];
    new_record.owning_atlas = owning_atlas;
    new_record.file_path = path.value_or("");
    new_record.file_name =
        std::filesystem::path(new_record.file_path).stem().string();

    stbrp_rect new_rect{};
    new_rect.id = id;
    new_rect.w = width;
    new_rect.h = height;

    AtlasContext &ctx = atlases.at(owning_atlas);
    ctx.rects.push_back(new_rect);

    Texture old_tex = std::move(ctx.atlas);
    ctx.atlas =
        Texture::create(nullptr, old_tex.width, old_tex.height, old_tex.format);

    if (stbrp_pack_rects(&ctx.context, ctx.rects.data(), ctx.rects.size())) {
        for (const stbrp_rect &rect : ctx.rects) {
            TextureRecord &record = tex_records.at(rect.id);
            if (rect.id != id)
                old_tex.copy_to(ctx.atlas, record.offset, {rect.x, rect.y},
                                {rect.w, rect.h});

            record.offset = glm::vec2(rect.x, rect.y);
            record.size = glm::vec2(rect.w, rect.h);
        }
    }

    ctx.atlas.set_subtexture((uint8_t *)data, new_record.offset,
                             new_record.size);
    ctx.atlas.gen_mipmaps();

    old_tex.destroy();

    return id;
}

} // namespace eng
