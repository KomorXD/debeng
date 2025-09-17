#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "eng/renderer/opengl.hpp"
#include <map>

namespace eng {

struct MeshInstance {
    glm::mat4 transform;
    float material_idx = 0.0f;
    float entity_id = 0.0f;
};

using AssetID = int32_t;

struct Mesh {
    std::string name;
    VertexArray vao;
};

struct Material {
    std::string name;

    glm::vec4 color = glm::vec4(1.0f);
    glm::vec2 tiling_factor = glm::vec2(1.0f);
    glm::vec2 texture_offset = glm::vec2(0.0f);

    AssetID shader_id = 1;
    AssetID albedo_texture_id = 1;
    AssetID normal_texture_id = 2;
};

struct AssetPack {
    [[nodiscard]] static AssetPack create(const std::string &pack_name);

    static constexpr AssetID QUAD_ID = 1;
    static constexpr AssetID CUBE_ID = 2;
    static constexpr AssetID SPHERE_ID = 3;

    static constexpr AssetID TEXTURE_WHITE = 1;
    static constexpr AssetID TEXTURE_NORMAL = 2;

    static constexpr AssetID DEFAULT_BASE_MATERIAL = 1;
    static constexpr AssetID DEFAULT_FLAT_MATERIAL = 2;

    static constexpr AssetID BASE_SHADER = 1;
    static constexpr AssetID FLAT_SHADER = 2;
    static constexpr AssetID SCREEN_QUAD_SHADER = 3;

    void destroy();

    [[nodiscard]] AssetID add_mesh(Mesh mesh);
    [[nodiscard]] AssetID add_texture(Texture &texture);
    [[nodiscard]] AssetID add_material(Material &material);
    [[nodiscard]] AssetID add_shader(Shader &shader);

    std::string name;
    std::map<AssetID, Mesh> meshes;
    std::map<AssetID, Texture> textures;
    std::map<AssetID, Material> materials;
    std::map<AssetID, Shader> shaders;
};

} // namespace eng

#endif
