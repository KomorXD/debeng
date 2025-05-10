#include "eng/timer.hpp"

void Timer::start() {
    start_timepoint = std::chrono::steady_clock::now();
    accumulated_time_ms = 0;
    running = true;
}

void Timer::stop() {
    if (!running)
        return;

    accumulated_time_ms +=
        std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                     start_timepoint).count() * 1000.0f;
    running = false;
}

void Timer::resume() {
    if (running)
        return;

    start_timepoint = std::chrono::steady_clock::now();
    running = true;
}

float Timer::elapsed_time_ms() {
    if (running){
        uint32_t diff = std::chrono::duration<float>(
                            std::chrono::steady_clock::now() - start_timepoint)
                            .count() * 1000.0f;
        return (accumulated_time_ms + diff);
    }

    return accumulated_time_ms;
}
