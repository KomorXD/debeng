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

    /*  "height - y" to flip coords */
    return { (float)pos_x, (float)height - (float)pos_y };
}

glm::vec2 get_mouse_move_delta() {
    static glm::vec2 prev_position(-1.0f);
    if (prev_position == glm::vec2(-1.0f))
        prev_position = get_mouse_position();

    glm::vec2 curr_position = get_mouse_position();
    glm::vec2 delta = curr_position - prev_position;
    prev_position = curr_position;

    return delta;
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
