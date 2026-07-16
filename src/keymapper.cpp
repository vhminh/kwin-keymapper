#include "keymapper.h"

#include "config.h"
#include "log.h"

#include <cassert>

KeyMapper::KeyMapper() : phys_mods(0), virt_mods(0) {}

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

ModMask apply_events_to_mods(ModMask mask, const std::vector<input_event>& events) {
    for (const input_event& ev : events) {
        mask = apply_event_to_mods(mask, ev);
    }
    return mask;
}

void KeyMapper::process_evdev_key(
    const Arc<Window>& active_window, const input_event& ev, std::vector<input_event>& result
) {
    this->phys_mods = apply_event_to_mods(this->phys_mods, ev);
    if (is_evdev_mod(ev.code)) {
        produce_mod_diff(this->virt_mods, this->phys_mods, ev.time, result);
    } else {
        auto [expected_virt_mods, expected_key] = user_key_map(active_window, phys_mods, ev.code);
        produce_mod_diff(this->virt_mods, expected_virt_mods, ev.time, result);
        // TODO: produce diff for key code too, not just for mods
        //       it's lucky that all the keymaps use the same alpha keys, just different mod :)
        result.push_back(input_event{.time = ev.time, .type = ev.type, .code = expected_key, .value = ev.value});
    }
    this->virt_mods = apply_events_to_mods(this->virt_mods, result);
}
