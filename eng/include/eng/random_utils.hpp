#ifndef RANDOM_UTILS_HPP
#define RANDOM_UTILS_HPP

#include <optional>
#include <string>

[[nodiscard]] std::optional<std::string> get_file_content(const std::string &path);

#endif
