#ifndef TRIGGER_TIMER_HPP
#define TRIGGER_TIMER_HPP

#include <functional>
#include <vector>

struct TriggerTimer {
    static std::vector<TriggerTimer *> timers;
    static void register_timer(TriggerTimer &timer);
    static void unregister_timer(TriggerTimer &timer);
    static void update_timers(float timestep_ms);

    void start();
    void stop();
    void resume();

    std::function<void(void)> func;
    float time_passed_ms = 0.0f;
    float interval_ms = 0.0f;
    bool running = false;
};

#endif
