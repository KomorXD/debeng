#ifndef RANDOM_UTILS_HPP
#define RANDOM_UTILS_HPP

#include <optional>
#include <string>

[[nodiscard]] std::optional<std::string> get_file_content(const std::string &path);

/* Replaces PATTERN with REPLACEMENT in place, for SOURCE.  */
void replace_all(std::string &source, const std::string &pattern,
                 const std::string &replacement);

#endif
