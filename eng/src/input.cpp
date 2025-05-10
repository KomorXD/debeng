#include "eng/input.hpp"
#include "GLFW/glfw3.h"

namespace eng {

bool is_key_pressed(Key key) {
    GLFWwindow *window = glfwGetCurrentContext();
    assert(window != nullptr && "Reading input without active window");

    return glfwGetKey(window, (int)key) == GLFW_PRESS;
}

bool is_mouse_btn_pressed(MouseButton button) {
    GLFWwindow *window = glfwGetCurrentContext();
    assert(window != nullptr && "Reading input without active window");

    return glfwGetMouseButton(window, (int)button) == GLFW_PRESS;
}

glm::vec2 get_mouse_position() {
    GLFWwindow *window = glfwGetCurrentContext();
    assert(window != nullptr && "Reading input without active window");

    double pos_x;
    double pos_y;
    glfwGetCursorPos(window, &pos_x, &pos_y);

    int width;
    int height;
    glfwGetWindowSize(window, &width, &height);

    return { (float)pos_x, (float)height - (float)pos_y };
}

void hide_cursor() {
    GLFWwindow *window = glfwGetCurrentContext();
    assert(window != nullptr && "Reading input without active window");

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

void show_cursor() {
    GLFWwindow *window = glfwGetCurrentContext();
    assert(window != nullptr && "Reading input without active window");

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void disable_cursor() {
    GLFWwindow *window = glfwGetCurrentContext();
    assert(window != nullptr && "Reading input without active window");

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void enable_cursor() {
    GLFWwindow *window = glfwGetCurrentContext();
    assert(window != nullptr && "Reading input without active window");

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

} // namespace eng
