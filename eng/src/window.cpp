#include "window.hpp"
#include "GLFW/glfw3.h"

bool Window::init() {
    return glfwInit() == GLFW_TRUE;
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

    return window;
}

void Window::terminate() {
    glfwTerminate();
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
