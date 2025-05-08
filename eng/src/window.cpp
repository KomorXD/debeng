#include "window.hpp"
#include "GLFW/glfw3.h"
#include <cstdio>

namespace eng {

bool Window::init() {
    return glfwInit() == GLFW_TRUE;
}

static void error_cb(int err_code, const char *description) {
    fprintf(stderr, "GLFW error #%d: %s\r\n", err_code, description);
}

static void key_cb(GLFWwindow *window, int key, int scancode, int action,
                   int mods) {
    Event ev{};
    switch (action) {
    case GLFW_PRESS:
        ev.type = EventType::KeyPressed;
        break;
    case GLFW_RELEASE:
        ev.type = EventType::KeyReleased;
        break;
    case GLFW_REPEAT:
        ev.type = EventType::KeyHeld;
        break;
    }

    ev.key.key = (Key)key;
    ev.key.alt = mods | GLFW_MOD_ALT;
    ev.key.shift = mods | GLFW_MOD_SHIFT;
    ev.key.ctrl = mods | GLFW_MOD_CONTROL;

    Window *owner = (Window *)glfwGetWindowUserPointer(window);
    owner->pending_events.push(ev);
}

static void cursor_pos_cb(GLFWwindow* window, double pos_x, double pos_y) {
    Event ev{};
    ev.type = EventType::MouseMoved;
    ev.mouse.pos_x = (float)pos_x;
    ev.mouse.pos_y = (float)pos_y;

    Window *owner = (Window *)glfwGetWindowUserPointer(window);
    owner->pending_events.push(ev);
}

static void mouse_btn_cb(GLFWwindow* window, int button, int action, int mods) {
    Event ev{};
    switch (action) {
    case GLFW_PRESS:
        ev.type = EventType::MouseButtonPressed;
        break;
    case GLFW_RELEASE:
        ev.type = EventType::MouseButtonReleased;
        break;
    case GLFW_REPEAT:
        ev.type = EventType::MouseButtonHeld;
        break;
    }

    ev.mouse_button.button = (MouseButton)button;
    ev.mouse_button.alt = mods | GLFW_MOD_ALT;
    ev.mouse_button.shift = mods | GLFW_MOD_SHIFT;
    ev.mouse_button.ctrl = mods | GLFW_MOD_CONTROL;

    Window *owner = (Window *)glfwGetWindowUserPointer(window);
    owner->pending_events.push(ev);
}

static void scroll_cb(GLFWwindow* window, double offset_x, double offset_y) {
    Event ev{};
    ev.type = EventType::MouseWheelScrolled;
    ev.mouse_scroll.offset_x = offset_x;
    ev.mouse_scroll.offset_y = offset_y;

    Window *owner = (Window *)glfwGetWindowUserPointer(window);
    owner->pending_events.push(ev);
}

static void window_size_cb(GLFWwindow *window, int width, int height) {
    Event ev{};
    ev.type = EventType::WindowResized;
    ev.window_size.width = width;
    ev.window_size.height = height;

    Window *owner = (Window *)glfwGetWindowUserPointer(window);
    owner->pending_events.push(ev);
}

static void set_window_callbacks(Window &window) {
    glfwSetErrorCallback(error_cb);
    glfwSetKeyCallback(window.handle, key_cb);
    glfwSetCursorPosCallback(window.handle, cursor_pos_cb);
    glfwSetMouseButtonCallback(window.handle, mouse_btn_cb);
    glfwSetScrollCallback(window.handle, scroll_cb);
    glfwSetWindowSizeCallback(window.handle, window_size_cb);
}

std::optional<Window> Window::create(const WindowSpec &spec) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, spec.maximized ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    Window window;
    window.handle = glfwCreateWindow(spec.width, spec.height,
                                     spec.title.c_str(), nullptr, nullptr);
    if (!window.handle) {
        return {};
    }

    window.spec = spec;

    /* Done in case the window was maximized, but provided size in spec was
    different. */
    glfwGetWindowSize(window.handle, &window.spec.width, &window.spec.height);

    glfwMakeContextCurrent(window.handle);
    glfwSwapInterval(spec.vsync_enabled ? 1 : 0);
    set_window_callbacks(window);

    return window;
}

void Window::terminate() {
    glfwTerminate();
}

void Window::update_user_pointer() {
    glfwSetWindowUserPointer(handle, this);
}

bool Window::is_open() const {
    return !glfwWindowShouldClose(handle);
}

void Window::update() {
    glfwPollEvents();
    glfwSwapBuffers(handle);
}

void Window::close() {
    if (!handle)
        return;

    glfwSetWindowShouldClose(handle, 1);
}

bool Window::is_key_pressed(Key key) {
    return glfwGetKey(handle, (int)key) == GLFW_PRESS;
}

bool Window::is_mouse_btn_pressed(MouseButton button) {
    return glfwGetMouseButton(handle, (int)button) == GLFW_PRESS;
}

void Window::hide_cursor() {
    glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

void Window::show_cursor() {
    glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Window::disable_cursor() {
    glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Window::enable_cursor() {
    glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

} // namespace eng
