#include "intercept.h"

EvDevInterceptor::EvDevInterceptor() : held_mods(Mod::None) {}

std::vector<input_event> EvDevInterceptor::process_evdev_key(const Window* active_window, const input_event& ev) {
    std::vector<input_event> result;
    result.push_back(ev);
    return result;
}
