#include "eng/random_utils.hpp"
#include <sstream>
#include <string>
#include <fstream>

std::optional<std::string> get_file_content(const std::string &path) {
    std::ifstream file(path);
    if (!file.good())
        return {};

    std::string line{};
    std::stringstream ss{};
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
