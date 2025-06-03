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
        pack.add_mesh(quad_mesh);
    }

    {
        Mesh cube_mesh = create_mesh(cube_vertex_data());
        cube_mesh.name = "Cube";
        pack.add_mesh(cube_mesh);
    }

    return pack;
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

} // namespace eng
