#include "eng/parsers/gltf.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/quaternion.hpp"
#include <optional>

#define JSON_ACCESS(statement)                                                 \
    try {                                                                      \
        statement;                                                             \
    } catch (const nlohmann::json::exception &e) {                             \
        fprintf(stderr, "Error parsing JSON: %s\n", e.what());                 \
        return false;                                                          \
    }

namespace eng::gltf {

using json = nlohmann::json;

static AccessorType type_str_to_type(const std::string &type_str) {
    std::unordered_map<std::string, AccessorType> types_map = {
        {"SCALAR",  AccessorType::SCALAR},
        {"VEC2",    AccessorType::VEC2},
        {"VEC3",    AccessorType::VEC3},
        {"VEC4",    AccessorType::VEC4},
        {"MAT4",    AccessorType::MAT4}};

    return types_map.at(type_str);
}

static bool parse_buffers(const nlohmann::json &buffers, GLTF_Model &model) {
    model.buffers.reserve(buffers.size());

    for (const json &buffer : buffers) {
        gltf::Buffer &buf = model.buffers.emplace_back();
        JSON_ACCESS(buf.URI = buffer.at("uri").get<std::string>());
        JSON_ACCESS(buf.byte_len = buffer.at("byteLength").get<size_t>());
    }

    return true;
}

static bool parse_buffer_views(const nlohmann::json &views, GLTF_Model &model) {
    model.buffer_views.reserve(views.size());

    for (const json &buffer_view : views) {
        gltf::BufferView &view = model.buffer_views.emplace_back();
        JSON_ACCESS(view.buffer_index =
                        buffer_view.at("buffer").get<int32_t>());
        JSON_ACCESS(view.byte_len = buffer_view.at("byteLength").get<size_t>());

        if (buffer_view.contains("byteOffset")) {
            JSON_ACCESS(view.byte_offset =
                            buffer_view.at("byteOffset").get<size_t>());
        }

        if (buffer_view.contains("byteStride")) {
            JSON_ACCESS(view.byte_stride =
                            buffer_view.at("byteStride").get<size_t>());
        }

        if (buffer_view.contains("target")) {
            JSON_ACCESS(view.target = buffer_view.at("target").get<int32_t>());
        }
    }

    return true;
}

static bool parse_accessors(const nlohmann::json &accessors,
                            GLTF_Model &model) {
    model.accessors.reserve(accessors.size());

    for (const json &accessor : accessors) {
        gltf::Accessor &acc = model.accessors.emplace_back();
        JSON_ACCESS(acc.buffer_view_index =
                        accessor.at("bufferView").get<int32_t>());

        if (accessor.contains("byteOffset")) {
            JSON_ACCESS(acc.byte_offset =
                            accessor.at("byteOffset").get<size_t>());
        }

        JSON_ACCESS(acc.component_type =
                        accessor.at("componentType").get<int32_t>());
        JSON_ACCESS(acc.count =
                        accessor.at("count").get<size_t>());

        std::string type;
        JSON_ACCESS(type = accessor.at("type").get<std::string>());
        acc.type = type_str_to_type(type);

        if (accessor.contains("min")) {
            JSON_ACCESS(
                acc.min = glm::make_vec3(
                    accessor.at("min").get<std::vector<float>>().data()));
        }

        if (accessor.contains("max")) {
            JSON_ACCESS(
                acc.max = glm::make_vec3(
                    accessor.at("max").get<std::vector<float>>().data()));
        }
    }

    return true;
}

static bool parse_meshes(const nlohmann::json &meshes, GLTF_Model &model) {
    model.meshes.reserve(meshes.size());

    for (const json &mesh : meshes) {
        gltf::Mesh &m = model.meshes.emplace_back();
        JSON_ACCESS(m.name = mesh.at("name").get<std::string>());

        json primitives;
        JSON_ACCESS(primitives = mesh.at("primitives"));
        if (!primitives.is_array())
            return false;

        for (const json &primitive : primitives) {
            gltf::Primitive &prim = m.primitives.emplace_back();

            json attributes;
            JSON_ACCESS(attributes = primitive.at("attributes"));
            JSON_ACCESS(prim.position_accessor_index =
                            attributes.at("POSITION").get<int32_t>());
            JSON_ACCESS(prim.normal_accessor_index =
                            attributes.at("NORMAL").get<int32_t>());
            JSON_ACCESS(prim.tex_coords_accessor_index =
                            attributes.at("TEXCOORD_0").get<int32_t>());
            JSON_ACCESS(prim.indices_accessor_index =
                            primitive.at("indices").get<int32_t>());

            if (primitive.contains("material")) {
                JSON_ACCESS(prim.material_index =
                                primitive.at("material").get<int32_t>());
            }
        }
    }

    return true;
}

static bool parse_nodes(const nlohmann::json &nodes, GLTF_Model &model) {
    model.nodes.reserve(nodes.size());

    for (const json &node : nodes) {
        gltf::Node &n = model.nodes.emplace_back();
        JSON_ACCESS(n.name = node.value("name", "[empty]"));

        if (node.contains("mesh"))
            JSON_ACCESS(n.mesh_index = node.at("mesh").get<int32_t>());

        if (node.contains("matrix")) {
            JSON_ACCESS(
                n.local_transform = glm::make_mat4(
                    node.at("matrix").get<std::vector<float>>().data()));
        } else {
            glm::vec3 translation(0.0f);
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale(1.0f);

            if (node.contains("translation")) {
                JSON_ACCESS(translation =
                                glm::make_vec3(node.at("translation")
                                                   .get<std::vector<float>>()
                                                   .data()));
            }

            if (node.contains("rotation")) {
                JSON_ACCESS(
                    rotation = glm::make_quat(
                        node.at("rotation").get<std::vector<float>>().data()));
            }

            if (node.contains("scale")) {
                JSON_ACCESS(
                    scale = glm::make_vec3(
                        node.at("scale").get<std::vector<float>>().data()));
            }

            n.local_transform = glm::translate(glm::mat4(1.0f), translation) *
                                glm::mat4_cast(rotation) *
                                glm::scale(glm::mat4(1.0f), scale);
        }

        if (node.contains("children")) {
            json children = node.at("children");
            for (const json &child : children) {
                int32_t &c = n.children_indices.emplace_back();
                JSON_ACCESS(c = child.get<int32_t>());
            }
        }
    }


    return true;
}

static bool parse_materials(const nlohmann::json &materials, GLTF_Model &model) {
    model.materials.reserve(materials.size());

    for (const json &material : materials) {
        gltf::Material &mat = model.materials.emplace_back();
        JSON_ACCESS(mat.name = material.value("name", "[empty]"));

        if (material.contains("normalTexture")) {
            JSON_ACCESS(
                mat.normal_texture_index =
                    material.at("normalTexture").at("index").get<int32_t>());
        }

        if (material.contains("emissiveTexture")) {
            JSON_ACCESS(
                mat.emission_texture_index =
                    material.at("emissiveTexture").at("index").get<int32_t>());
        }

        if (material.contains("emissiveFactor")) {
            JSON_ACCESS(mat.emission_color =
                            glm::make_vec3(material.at("emissiveFactor")
                                               .get<std::vector<float>>()
                                               .data()));
        }

        if (material.contains("pbrMetallicRoughness")) {
            json pbr;
            JSON_ACCESS(pbr = material.at("pbrMetallicRoughness"));

            if (pbr.contains("baseColorFactor")) {
                JSON_ACCESS(mat.albedo =
                                glm::make_vec4(pbr.at("baseColorFactor")
                                                   .get<std::vector<float>>()
                                                   .data()));
            }

            if (pbr.contains("baseColorTexture")) {
                JSON_ACCESS(
                    mat.albedo_texture_index =
                        pbr.at("baseColorTexture").at("index").get<int32_t>());
            }

            if (pbr.contains("metallicRoughnessTexture")) {
                JSON_ACCESS(mat.orm_texture_index =
                                pbr.at("metallicRoughnessTexture")
                                    .at("index")
                                    .get<int32_t>());
            }

            JSON_ACCESS(mat.roughness = pbr.value("roughnessFactor", 1.0f));
            JSON_ACCESS(mat.metallic = pbr.value("metallicFactor", 1.0f));
        }
    }

    return true;
}

static bool parse_images(const nlohmann::json &images, GLTF_Model &model) {
    model.images.reserve(images.size());

    for (const json &image : images) {
        gltf::Image &img = model.images.emplace_back();
        JSON_ACCESS(img.URI = image.at("uri").get<std::string>());
    }

    return true;
}

static bool parse_samplers(const nlohmann::json &samplers, GLTF_Model &model) {
    model.samplers.reserve(samplers.size());

    for (const json &sampler : samplers) {
        gltf::Sampler &smp = model.samplers.emplace_back();
        JSON_ACCESS(smp.min_filter = sampler.value("minFilter", GL_LINEAR));
        JSON_ACCESS(smp.mag_filter = sampler.value("magFilter", GL_LINEAR));
        JSON_ACCESS(smp.wrap_s = sampler.value("wrapS", GL_REPEAT));
        JSON_ACCESS(smp.wrap_t = sampler.value("wrapT", GL_REPEAT));
    }

    return true;
}

static bool parse_textures(const nlohmann::json &textures, GLTF_Model &model) {
    model.textures.reserve(textures.size());

    for (const json &texture : textures) {
        gltf::Texture &tex = model.textures.emplace_back();
        JSON_ACCESS(tex.image_index = texture.at("source").get<int32_t>());
        JSON_ACCESS(tex.sampler_index = texture.at("sampler").get<int32_t>());
    }

    return true;
}

#define EARLY_RET(condition)                                                   \
    if (!(condition))                                                          \
        return std::nullopt;

std::optional<GLTF_Model> parse_source(const nlohmann::json &source) {
    GLTF_Model out;

    json buffers = source.at("buffers");
    EARLY_RET(buffers.is_array());
    EARLY_RET(parse_buffers(buffers, out));

    json views = source.at("bufferViews");
    EARLY_RET(views.is_array());
    EARLY_RET(parse_buffer_views(views, out));

    json accessors = source.at("accessors");
    EARLY_RET(accessors.is_array());
    EARLY_RET(parse_accessors(accessors, out));

    json meshes = source.at("meshes");
    EARLY_RET(meshes.is_array());
    EARLY_RET(parse_meshes(meshes, out));

    json nodes = source.at("nodes");
    EARLY_RET(nodes.is_array());
    EARLY_RET(parse_nodes(nodes, out));

    json materials = source.at("materials");
    EARLY_RET(materials.is_array());
    EARLY_RET(parse_materials(materials, out));

    json images = source.at("images");
    EARLY_RET(images.is_array());
    EARLY_RET(parse_images(images, out));

    json samplers = source.at("samplers");
    EARLY_RET(samplers.is_array());
    EARLY_RET(parse_samplers(samplers, out));

    json textures = source.at("textures");
    EARLY_RET(textures.is_array());
    EARLY_RET(parse_textures(textures, out));

    return out;
}

std::optional<Model> build_model(const nlohmann::json &source) {
    std::optional<GLTF_Model> model_opt = parse_source(source);
    [[maybe_unused]] GLTF_Model model = std::move(model_opt.value());

    return std::nullopt;
}

};
