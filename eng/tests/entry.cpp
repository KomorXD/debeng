#include <gtest/gtest.h>

#include "eng/window.hpp"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);

    eng::Window::init();
    int ret = RUN_ALL_TESTS();
    eng::Window::terminate();

    return ret;
}
