#include "eng/random_utils.hpp"
#include "glm/gtc/quaternion.hpp"
#include <sstream>
#include <string>
#include <fstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

std::optional<std::string> get_file_content(const std::string &path) {
    std::ifstream file(path);
    if (!file.good())
        return std::nullopt;

    std::string line;
    std::stringstream ss;
    while (std::getline(file, line)) {
        ss << line << '\n';
    }

    return ss.str();
}

void replace_all(std::string &source, const std::string &pattern,
                 const std::string &replacement) {
    if (pattern.empty())
        return;

    size_t start = std::string::npos;
    while ((start = source.find(pattern)) != std::string::npos) {
        source.replace(start, pattern.length(), replacement);
        start += pattern.length();
    }
}

void transform_decompose(const glm::mat4 &transform, glm::vec3 &translation,
                         glm::vec3 &rotation, glm::vec3 &scale) {
    glm::quat orientation;
    glm::vec3 dummy_skew;
    glm::vec4 dummy_perspevtive;

    bool success = glm::decompose(transform, scale, orientation, translation,
                                  dummy_skew, dummy_perspevtive);
    (void)success;
    assert(success && "Couldn't decompose transform matrix");

    rotation = glm::eulerAngles(orientation);
}
