#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "eng/renderer/opengl.hpp"
#include <map>

namespace eng {

struct MeshInstance {
    glm::mat4 transform;
};

using AssetID = int32_t;

struct Mesh {
    AssetID id;
    std::string name;
    VertexArray vao;
};

struct Material {
    std::string name;

    glm::vec4 color = glm::vec4(1.0f);
    glm::vec2 tiling_factor = glm::vec2(1.0f);
    glm::vec2 texture_offset = glm::vec2(0.0f);

    AssetID albedo_texture_id = 1;
};

struct AssetPack {
    [[nodiscard]] static AssetPack create(const std::string &pack_name);

    static constexpr AssetID QUAD_ID = 1;
    static constexpr AssetID CUBE_ID = 2;
    static constexpr AssetID SPHERE_ID = 3;

    static constexpr AssetID TEXTURE_WHITE = 1;

    static constexpr AssetID DEFAULT_MATERIAL = 1;

    void destroy();

    [[nodiscard]] Mesh &add_mesh(Mesh mesh);
    [[nodiscard]] Texture &add_texture(Texture &texture);
    [[nodiscard]] Material &add_material(Material &material);

    std::string name;
    std::map<AssetID, Mesh> meshes;
    std::map<AssetID, Texture> textures;
    std::map<AssetID, Material> materials;
};

} // namespace eng

#endif
