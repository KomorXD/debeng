#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <cstdint>
#include <optional>
#include <string>

struct GLFWwindow;

namespace eng {

struct WindowSpec {
    int32_t width = 1280;
    int32_t height = 720;
    std::string title = "xdd";

    bool maximized = false;
    bool vsync_enabled = false;
};

struct Window {
    // Initializes GLFW context, must be done before creating the first window.
    static bool init();
    static std::optional<Window> create(const WindowSpec &spec);

    // Terminates GLFW context and destroys every window
    static void terminate();

    bool is_open() const;
    void update();
    void close();

    GLFWwindow *handle = nullptr;
    WindowSpec spec{};
};

} // namespace egg

#endif
