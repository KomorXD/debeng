#ifndef RANDOM_UTILS_HPP
#define RANDOM_UTILS_HPP

#include "glm/fwd.hpp"
#include <optional>
#include <string>

[[nodiscard]] std::optional<std::string> get_file_content(const std::string &path);

/* Replaces PATTERN with REPLACEMENT in place, for SOURCE.  */
void replace_all(std::string &source, const std::string &pattern,
                 const std::string &replacement);

bool transform_decompose(const glm::mat4 &transform, glm::vec3 &translation,
                         glm::vec3 &rotation, glm::vec3 &scale);

/* Generic error, either success or failure. */
enum class GenericError {
    NO_ERROR,
    ERROR
};

/* Generic result type - not stored as an union, but as a basic struct of
 * expected value type and error type. Defined error type should be able to
 * communicate that no errors occured, so no-error, in which case we should be
 * confident that the value is valid. */
template<typename ValueType, typename ErrorType = GenericError>
struct Result {
    ValueType value;
    ErrorType error;
};

#endif
