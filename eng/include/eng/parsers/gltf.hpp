#ifndef GLTF_HPP
#define GLTF_HPP

#include "eng/scene/assets.hpp"
#include "nlohmann/json.hpp"

namespace eng::gltf {

struct Buffer {
    std::string URI;
    size_t byte_len{};
};

struct BufferView {
    int32_t buffer_index{};
    size_t byte_offset = 0;
    size_t byte_len{};
    std::optional<size_t> byte_stride;
    std::optional<GLenum> target;
};

enum class AccessorType {
    SCALAR,
    VEC2,
    VEC3,
    VEC4,
    MAT4,

    COUNT
};

struct Accessor {
    int32_t buffer_view_index{};
    size_t byte_offset{};
    GLenum component_type{};
    size_t count{};
    AccessorType type{};
    std::optional<glm::vec3> min;
    std::optional<glm::vec3> max;
};

struct Primitive {
    int32_t position_accessor_index{};
    int32_t normal_accessor_index{};
    int32_t tex_coords_accessor_index{};
    int32_t indices_accessor_index{};
    std::optional<int32_t> material_index;
};

struct Mesh {
    std::string name;
    std::vector<Primitive> primitives;
};

struct Node {
    std::string name;
    std::optional<int32_t> mesh_index;
    glm::mat4 local_transform;

    std::vector<int32_t> children_indices;
};

struct Material {
    std::string name;

    glm::vec4 albedo{1.0f};
    glm::vec3 emission_color{0.0f};
    float roughness = 1.0f;
    float metallic = 1.0f;

    std::optional<int32_t> albedo_texture_index{};
    std::optional<int32_t> normal_texture_index{};
    std::optional<int32_t> orm_texture_index{};
    std::optional<int32_t> emission_texture_index{};
};

struct Image {
    std::string URI;
};

struct Sampler {
    GLenum min_filter{};
    GLenum mag_filter{};
    GLenum wrap_s{};
    GLenum wrap_t{};
};

struct Texture {
    int32_t image_index{};
    int32_t sampler_index{};
};

struct GLTF_Model {
    std::vector<gltf::Buffer> buffers;
    std::vector<gltf::BufferView> buffer_views;
    std::vector<gltf::Accessor> accessors;
    std::vector<gltf::Mesh> meshes;
    std::vector<gltf::Node> nodes;
    std::vector<gltf::Material> materials;
    std::vector<gltf::Texture> textures;
    std::vector<gltf::Image> images;
    std::vector<gltf::Sampler> samplers;
};

std::optional<GLTF_Model> parse_source(const nlohmann::json &source);

std::optional<Model> build_model(const nlohmann::json &source);

};

#endif
