#include <cstdio>
#include <cstdlib>
#include "eng/test.hpp"
#include "GLFW/glfw3.h"

const char *get_user() {
    char *user = getenv("USER");
    return user;
}

void thing() {
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "aha\r\n");
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow *window = glfwCreateWindow(1280, 720, "xdd", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "gg\r\n");
        return;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
