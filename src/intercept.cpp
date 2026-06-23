#include "intercept.h"

#include "config.h"

EvDevInterceptor::EvDevInterceptor() : held_mods(Mod::NONE) {}

std::vector<input_event> EvDevInterceptor::process_evdev_key(const Arc<Window>& active_window, const input_event& ev) {
    std::vector<input_event> result;
    result.push_back(ev);
    return result;
}
