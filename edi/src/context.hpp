#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "eng/window.hpp"
#include "layers.hpp"
#include "result.hpp"
#include <stack>

enum class ContextError {
    NONE,
    GLFW_FAIL,
    WINDOW_FAIL,
    RENDERER_FAIL
};

struct Context {
    static Result<Context *, ContextError> create();

    void close_app();
    void cleanup();
    [[nodiscard]] uint32_t fps();

    void run_loop();

    void push_layer(std::unique_ptr<Layer> &&layer);
    void pop_layer();

    /*  Don't modify directly - use push/pop_layer, so run_loop works fine. */
    std::stack<std::unique_ptr<Layer>> layers;

    eng::Window main_window;
    float timestep = 1.0f / 60.0f;
};

[[nodiscard]] Context *context();

#endif // CONTEXT_HPP
