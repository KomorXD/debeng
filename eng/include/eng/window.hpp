#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <cstdint>
#include <optional>
#include <queue>
#include <string>

#include "event.hpp"

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

    /* Necessary to do each time the location of window object changes, so
       that events are caught properly. */
    void update_user_pointer();
    bool is_open() const;
    void update();
    void close();

    bool is_key_pressed(Key key);
    bool is_mouse_btn_pressed(MouseButton button);
    void hide_cursor();
    void show_cursor();
    void disable_cursor();
    void enable_cursor();

    void set_title(const std::string &title);

    GLFWwindow *handle = nullptr;
    WindowSpec spec{};

    /* Window's pending events that should be cleared and checked each
       frame. */
    std::queue<Event> pending_events;
};

} // namespace eng

#endif
