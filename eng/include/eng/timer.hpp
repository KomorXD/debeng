#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>

struct Timer {
    void start();
    void stop();
    void resume();

    [[nodiscard]] float elapsed_time_ms();

    std::chrono::steady_clock::time_point start_timepoint{};
    uint32_t accumulated_time_ms = 0;
    bool running = false;
};

#endif
