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

struct AssetPack {
    [[nodiscard]] static AssetPack create(const std::string &pack_name);

    static constexpr AssetID QUAD_ID = 1;
    static constexpr AssetID CUBE_ID = 2;
    static constexpr AssetID SPHERE_ID = 3;

    void destroy();

    [[nodiscard]] Mesh &add_mesh(Mesh mesh);

    std::string name;
    std::map<AssetID, Mesh> meshes;
};

} // namespace eng

#endif
