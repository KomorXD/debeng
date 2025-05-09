#include <gtest/gtest.h>
#include <thread>

#include "eng/timer.hpp"

using namespace std::chrono_literals;

#define MAX_ERROR_IN_MS 15.0f

TEST(Timer, MeasuringTime) {
    Timer t;
    t.start();
    std::this_thread::sleep_for(500ms);

    ASSERT_NEAR(t.elapsed_time_ms(), 500.0f, MAX_ERROR_IN_MS)
        << "Time measure error.";
}

TEST(Timer, TimerStopping) {
    Timer t;
    t.start();
    std::this_thread::sleep_for(150ms);
    t.stop();
    std::this_thread::sleep_for(150ms);

    ASSERT_NEAR(t.elapsed_time_ms(), 150.0f, MAX_ERROR_IN_MS)
        << "Timer did not stop properly";
}

TEST(Timer, TimerResuming) {
    Timer t;
    t.start();
    std::this_thread::sleep_for(150ms);
    t.stop();
    std::this_thread::sleep_for(150ms);

    EXPECT_NEAR(t.elapsed_time_ms(), 150.0f, MAX_ERROR_IN_MS)
        << "Timer did not stop when expected";

    t.resume();
    std::this_thread::sleep_for(150ms);

    ASSERT_NEAR(t.elapsed_time_ms(), 300.0f, MAX_ERROR_IN_MS)
        << "Timer did not resume properly";
}

TEST(Timer, StartingAgain) {
    Timer t;
    t.start();
    std::this_thread::sleep_for(150ms);
    t.start();
    std::this_thread::sleep_for(150ms);

    ASSERT_NEAR(t.elapsed_time_ms(), 150.0f, MAX_ERROR_IN_MS)
        << "Timer did not reset properly";
}
