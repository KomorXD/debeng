#include "context.hpp"

int main(int argc, char **argv) {
    Result<Context *, ContextError> context_res = Context::create();
    const char *error_msgs[] = {"Failed to initialize windowing system\n",
                                "Failed to create a window\n",
                                "Failed to load GL loader\n"};

    if (context_res.error != ContextError::NONE) {
        fprintf(stderr, "%s", error_msgs[(int32_t)context_res.error - 1]);
        return (int32_t)context_res.error;
    }

    Context *ctx = context_res.value;
    ctx->push_layer(EditorLayer::create(ctx->main_window.spec));
    ctx->run_loop();
    ctx->cleanup();

    return 0;
}
