#include "intercept.h"

#include "config.h"
#include "log.h"

#include <cassert>

EvDevInterceptor::EvDevInterceptor() : phys_mods(0), virt_mods(0) {}

Mod evdev_to_mod(u16 evdev_key) {
    switch (evdev_key) {
    case KEY_LEFTCTRL:
        return LEFT_CTRL;
    case KEY_RIGHTCTRL:
        return RIGHT_CTRL;
    case KEY_LEFTSHIFT:
        return LEFT_SHIFT;
    case KEY_RIGHTSHIFT:
        return RIGHT_SHIFT;
    case KEY_LEFTALT:
        return LEFT_ALT;
    case KEY_RIGHTALT:
        return RIGHT_ALT;
    }
    return NONE;
}

u16 mod_to_evdev(Mod mod) {
    assert(mod != Mod::NONE);
    switch (mod) {
    case NONE:
        UNREACHABLE("mod must not be NONE");
    case LEFT_CTRL:
        return KEY_LEFTCTRL;
    case RIGHT_CTRL:
        return KEY_RIGHTCTRL;
    case LEFT_SHIFT:
        return KEY_LEFTSHIFT;
    case RIGHT_SHIFT:
        return KEY_RIGHTSHIFT;
    case LEFT_ALT:
        return KEY_LEFTALT;
    case RIGHT_ALT:
        return KEY_RIGHTALT;
    }
    UNREACHABLE("invalid mod: {}", static_cast<u16>(mod));
}

bool is_evdev_mod(u16 evdev_key) {
    return evdev_to_mod(evdev_key) != Mod::NONE;
}

void produce_mod_diff(ModMask current, ModMask expected, timeval time, std::vector<input_event>& res) {
    for (Mod m : all_mods) {
        if ((current & m) && ((expected & m) == 0)) {
            res.push_back(input_event{.time = time, .type = EV_KEY, .code = mod_to_evdev(m), .value = 0});
        } else if (((current & m) == 0) && (expected & m)) {
            res.push_back(input_event{.time = time, .type = EV_KEY, .code = mod_to_evdev(m), .value = 1});
        }
    }
}

ModMask apply_event_to_mods(ModMask mask, const input_event& ev) {
    if (!is_evdev_mod(ev.code)) {
        return mask;
    }
    Mod mod = evdev_to_mod(ev.code);
    if (ev.value == 0) {
        // released
        return mask & (~mod);
    }
    if (ev.value == 1) {
        // pressed
        return mask | mod;
    }
    return mask;
}

std::tuple<ModMask, ModMask> apply_events_to_mods(
    ModMask phys, ModMask virt, const input_event& in, const std::vector<input_event>& out
) {
    phys = apply_event_to_mods(phys, in);
    for (const input_event& ev : out) {
        virt = apply_event_to_mods(virt, ev);
    }
    return std::make_tuple(phys, virt);
}

std::vector<input_event> EvDevInterceptor::process_evdev_key(const Arc<Window>& active_window, const input_event& ev) {
    std::vector<input_event> result;
    if (is_evdev_mod(ev.code)) {
        produce_mod_diff(this->virt_mods, this->phys_mods, ev.time, result);
        result.push_back(ev);
    } else {
        auto [expected_virt_mods, expected_key] = user_key_map(active_window, phys_mods, ev.code);
        produce_mod_diff(this->virt_mods, expected_virt_mods, ev.time, result);
        result.push_back(input_event{.time = ev.time, .type = ev.type, .code = expected_key, .value = ev.value});
    }
    std::tie(this->phys_mods, this->virt_mods) = apply_events_to_mods(this->phys_mods, this->virt_mods, ev, result);
    return result;
}
