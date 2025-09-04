#include "eng/scene/assets.hpp"
#include "eng/renderer/primitives.hpp"

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

    vbo = VertexBuffer::create();
    vbo.allocate(nullptr, 4096 * sizeof(MeshInstance));
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
        (void)pack.add_texture(default_albedo);
    }

    {
        Material def_mat;
        def_mat.name = "Default material";
        (void)pack.add_material(def_mat);
    }

    return pack;
}

void AssetPack::destroy() {
    for (auto &[mesh_id, mesh] : meshes)
        mesh.vao.destroy();

    for (auto &[tex_id, texture] : textures)
        texture.destroy();

    meshes.clear();
}

Mesh &AssetPack::add_mesh(Mesh mesh) {
    if (meshes.empty())
        mesh.id = 1;
    else
        mesh.id = meshes.rbegin()->first + 1;

    Mesh &new_mesh = meshes[mesh.id];
    new_mesh = mesh;
    return new_mesh;
}

Texture &AssetPack::add_texture(Texture &texture) {
    AssetID id = 0;
    if (textures.empty())
        id = 1;
    else
        id = textures.rbegin()->first + 1;

    Texture &new_texture = textures[id];
    new_texture = texture;
    return new_texture;
}

Material &AssetPack::add_material(Material &material) {
    AssetID id = 0;
    if (materials.empty())
        id = 1;
    else
        id = materials.rbegin()->first + 1;

    Material &new_material = materials[id];
    new_material = material;
    return new_material;
}

} // namespace eng
