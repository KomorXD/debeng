#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include "eng/scene/assets.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include <string>

namespace eng {

struct Name {
    std::string name;
};

struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    glm::mat4 to_mat4() const;
};

struct MeshComp {
    AssetID id = 1;
};

struct MaterialComp {
    AssetID id = 1;
};

struct PointLight {
    glm::vec3 color{1.0f};
    float intensity = 1.0f;

    float linear = 0.09f;
    float quadratic = 0.032f;
};

struct DirLight {
    glm::vec3 color{1.0f};
};

struct SpotLight {
    glm::vec3 color{1.0f};
    float intensity = 1.0f;

    float cutoff = 12.5f;
    float edge_smoothness = 0.0f;

    float linear = 0.22f;
    float quadratic = 0.2f;
};

} // namespace eng

#endif
