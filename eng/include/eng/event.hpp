#ifndef EVENT_HPP
#define EVENT_HPP

#include <cstdint>

namespace eng {

struct ResizeEvent {
    int32_t width = 0;
    int32_t height = 0;
};

struct KeyEvent {
    int code = 0;
    bool alt = false;
    bool shift = false;
    bool ctrl = false;
};

struct MouseMoveEvent {
    float pos_x = 0.0f;
    float pos_y = 0.0f;
};

struct MouseButtonEvent {
    int button = 0;
    bool alt = false;
    bool shift = false;
    bool ctrl = false;
};

struct MouseScrollEvent {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

enum class EventType {
    None,
    WindowResized,
    KeyPressed,
    KeyReleased,
    KeyHeld,
    MouseMoved,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseButtonHeld,
    MouseWheelScrolled
};

struct Event {
    EventType type = EventType::None;
    union {
        ResizeEvent window_size;
        KeyEvent key;
        MouseMoveEvent mouse;
        MouseButtonEvent mouse_button;
        MouseScrollEvent mouse_scroll;
    };
};

} // namespace eng

#endif
