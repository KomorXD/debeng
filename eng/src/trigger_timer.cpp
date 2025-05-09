#include "trigger_timer.hpp"
#include <cassert>
#include <algorithm>
#include <vector>

std::vector<TriggerTimer *> TriggerTimer::timers;

void TriggerTimer::update_timers(float timestep_ms) {
    for (TriggerTimer *timer : timers) {
        if (!timer->running)
            continue;

        timer->time_passed_ms += timestep_ms;
        while (timer->time_passed_ms >= timer->interval_ms) {
            timer->func();
            timer->time_passed_ms -= timer->interval_ms;
        }
    }
}

void TriggerTimer::register_timer(TriggerTimer &timer) {
    assert(std::find(timers.begin(), timers.end(), &timer) == timers.end() &&
           "Trigger timer is already registered");

    timers.push_back(&timer);
}

void TriggerTimer::unregister_timer(TriggerTimer &timer) {
    assert(std::erase(timers, &timer) != 0 &&
           "Trying to erase non-registered trigger timer");
}

void TriggerTimer::start() {
    running = true;
    time_passed_ms = 0.0f;
}

void TriggerTimer::stop() {
    running = false;
}

void TriggerTimer::resume() {
    running = true;
}
