#include <gtest/gtest.h>

#include "eng/trigger_timer.hpp"

TEST(TriggerTimer, TriggerFiring) {
    bool fired = false;
    TriggerTimer tt;
    TriggerTimer::register_timer(tt);
    tt.func = [&]() { fired = true; };
    tt.interval_ms = 150.0f;
    tt.start();

    TriggerTimer::update_timers(0.0f);
    ASSERT_FALSE(fired) << "Trigger fired too early";

    TriggerTimer::update_timers(151.0f);
    ASSERT_TRUE(fired) << "Trigger did not fire";

    TriggerTimer::unregister_timer(tt);
}

TEST(TriggerTimer, TimerStopping) {
    bool fired = false;
    TriggerTimer tt;
    TriggerTimer::register_timer(tt);
    tt.func = [&]() { fired = true; };
    tt.interval_ms = 150.0f;
    tt.start();

    TriggerTimer::update_timers(80.0f);
    ASSERT_FALSE(fired) << "Trigger fired too early";

    tt.stop();
    TriggerTimer::update_timers(100.0f);
    ASSERT_FALSE(fired) << "Trigger fired when stopped";

    tt.resume();
    TriggerTimer::update_timers(100.0f);
    ASSERT_TRUE(fired) << "Trigger did not fire";

    TriggerTimer::unregister_timer(tt);
}

TEST(TriggerTimer, TimerRestarting) {
    bool fired = false;
    TriggerTimer tt;
    TriggerTimer::register_timer(tt);
    tt.func = [&]() { fired = true; };
    tt.interval_ms = 150.0f;
    tt.start();

    TriggerTimer::update_timers(80.0f);
    ASSERT_FALSE(fired) << "Trigger fired too early";

    tt.start();
    TriggerTimer::update_timers(100.0f);
    ASSERT_FALSE(fired) << "Trigger fired even though it was restarted";

    TriggerTimer::update_timers(51.0f);
    ASSERT_TRUE(fired) << "Trigger did not fire";

    TriggerTimer::unregister_timer(tt);
}

TEST(TriggerTimer, TimerCatchingUp) {
    int32_t acc = 0;
    TriggerTimer tt;
    TriggerTimer::register_timer(tt);
    tt.func = [&]() { acc++; };
    tt.interval_ms = 150.0f;
    tt.start();

    TriggerTimer::update_timers(500.0f);
    ASSERT_EQ(acc, 3) << "Trigger did not catch up after a big delay";

    TriggerTimer::unregister_timer(tt);
}
