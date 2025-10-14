#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

#include "eng/scene/components.hpp"
#include "glm/ext/matrix_transform.hpp"

glm::mat4 eng::Transform::to_mat4() const {
    return glm::translate(glm::mat4(1.0f), position) *
           glm::toMat4(glm::quat(rotation)) *
           glm::scale(glm::mat4(1.0f), scale);
}

glm::mat4 eng::GlobalTransform::to_mat4() const {
    return glm::translate(glm::mat4(1.0f), position) *
           glm::toMat4(glm::quat(rotation)) *
           glm::scale(glm::mat4(1.0f), scale);
}
