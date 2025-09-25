#ifndef ASSETS_HPP
#define ASSETS_HPP

#include "eng/renderer/opengl.hpp"
#include "stb_rect_pack/stb_rect_pack.h"
#include <map>

namespace eng {

struct MeshInstance {
    glm::mat4 transform;
    float material_idx = 0.0f;
    float entity_id = 0.0f;
    float color_sens = 1.0f;
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

    AssetID albedo_tex_record_id = 1;
    AssetID normal_tex_record_id = 2;

    AssetID roughness_tex_record_id = 3;
    float roughness = 0.5f;

    AssetID metallic_tex_record_id = 3;
    float metallic = 0.1f;

    AssetID ao_tex_record_id = 3;
    float ao = 1.0f;
};

struct AtlasContext {
    Texture atlas;

    stbrp_context context{};
    std::vector<stbrp_rect> rects;
    std::vector<stbrp_node> nodes;
};

struct TextureRecord {
    AssetID owning_atlas = 1;
    glm::vec2 size{1.0};
    glm::vec2 offset{1.0};
    int32_t layer = 0;

    std::string file_path;
    std::string file_name;
};

struct AssetPack {
    [[nodiscard]] static AssetPack create(const std::string &pack_name);

    static constexpr AssetID QUAD_ID = 1;
    static constexpr AssetID CUBE_ID = 2;
    static constexpr AssetID SPHERE_ID = 3;

    static constexpr AssetID RGBA_ATLAS = 1;
    static constexpr AssetID RGB_ATLAS = 2;
    static constexpr AssetID R_ATLAS = 3;

    static constexpr AssetID TEXTURE_WHITE = 1;
    static constexpr AssetID TEXTURE_NORMAL = 2;
    static constexpr AssetID TEXTURE_WHITE_R = 3;

    static constexpr AssetID ATLAS_RGBA8 = 1;
    static constexpr AssetID ATLAS_RGB8 = 2;
    static constexpr AssetID ATLAS_R8 = 3;

    static constexpr AssetID DEFAULT_BASE_MATERIAL = 1;
    static constexpr AssetID DEFAULT_FLAT_MATERIAL = 2;

    static constexpr AssetID BASE_SHADER = 1;
    static constexpr AssetID FLAT_SHADER = 2;
    static constexpr AssetID SCREEN_QUAD_SHADER = 3;

    void destroy();

    [[nodiscard]] AssetID add_mesh(Mesh mesh);
    [[nodiscard]] AssetID add_material(Material &material);
    [[nodiscard]] AssetID add_shader(Shader &shader);

    [[nodiscard]] AssetID add_texture(const void *data, int32_t width,
                                      int32_t height, TextureFormat format,
                                      std::optional<std::string> path = {});

    std::string name;

    std::map<AssetID, Mesh> meshes;
    std::map<AssetID, Material> materials;
    std::map<AssetID, Shader> shaders;
    std::map<AssetID, TextureRecord> tex_records;
    std::map<AssetID, AtlasContext> atlases;
};

} // namespace eng

#endif
