#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

namespace eng {

struct Transform {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    glm::mat4 to_mat4() const;
};

} // namespace eng

#endif
