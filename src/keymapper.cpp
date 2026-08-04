#include "keymapper.h"

#include "config.h"
#include "log.h"

#include <bit>
#include <cassert>

// reset lowest set bit
u64 blsr(u64 value) {
    return value & (value - 1);
}

KeyMapper::KeyMapper() : phys_mods(0), virt_mods(0), phys_alphas{}, virt_alphas{} {}

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

bool is_exactly_one_alpha_key_pressed(AlphaSet set) {
    int cnt = 0;
    for (size_t i = 0; i < KEY_WORDS; ++i) {
        cnt += std::popcount(set[i]);
    }
    return cnt == 1;
}

AlphaSet single_alpha_key_set(u16 key) {
    AlphaSet set{0};
    set[key / 64] |= (u64(1) << (key % 64));
    return set;
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

void emit_bits(u64 bits, size_t i, timeval time, i32 pressed, std::vector<input_event>& res) {
    for (; bits; bits = blsr(bits)) {
        u16 code = static_cast<u16>(i * 64 + std::countr_zero(bits));
        res.push_back(input_event{.time = time, .type = EV_KEY, .code = code, .value = pressed});
    }
}

void produce_alpha_diff(AlphaSet current, AlphaSet expected, timeval time, std::vector<input_event>& res) {
    for (size_t i = 0; i < KEY_WORDS; ++i) {
        u64 releases = (current[i] & ~expected[i]);
        if (releases != 0) {
            emit_bits(releases, i, time, 0, res);
        }
    }
    for (size_t i = 0; i < KEY_WORDS; ++i) {
        u64 presses = (expected[i] & ~current[i]);
        if (presses != 0) {
            emit_bits(presses, i, time, 1, res);
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

AlphaSet apply_event_to_alphas(AlphaSet set, const input_event& ev) {
    if (is_evdev_mod(ev.code)) {
        return set;
    }
    if (ev.value == 0) {
        // released
        set[ev.code / 64] &= ~(u64(1) << (ev.code % 64));
        return set;
    }
    if (ev.value == 1) {
        // pressed
        set[ev.code / 64] |= (u64(1) << (ev.code % 64));
        return set;
    }
    return set;
}

AlphaSet apply_events_to_alphas(AlphaSet set, const std::vector<input_event>& events) {
    for (const input_event& ev : events) {
        set = apply_event_to_alphas(set, ev);
    }
    return set;
}

void KeyMapper::process_evdev_key(
    const Box<Window>& active_window, const input_event& ev, std::vector<input_event>& result
) {
    this->phys_mods = apply_event_to_mods(this->phys_mods, ev);
    this->phys_alphas = apply_event_to_alphas(this->phys_alphas, ev);
    bool new_alpha_key_pressed = !is_evdev_mod(ev.code) && (ev.value == 1 || ev.value == 2);

    u16 expected_virt_mods = this->phys_mods, expected_virt_key = ev.code;
    if (new_alpha_key_pressed && is_exactly_one_alpha_key_pressed(this->phys_alphas)) {
        std::tie(expected_virt_mods, expected_virt_key) = user_key_map(active_window, phys_mods, ev.code);
    }
    bool should_remap =
        new_alpha_key_pressed && (expected_virt_mods != this->phys_mods || expected_virt_key != ev.code);
    if (should_remap) {
        // make the virt state matches the user defined mapping
        produce_mod_diff(this->virt_mods, expected_virt_mods, ev.time, result);
        produce_alpha_diff(this->virt_alphas, single_alpha_key_set(expected_virt_key), ev.time, result);
    } else {
        // make the virt state matches the phys state
        produce_mod_diff(this->virt_mods, this->phys_mods, ev.time, result);
        produce_alpha_diff(this->virt_alphas, this->phys_alphas, ev.time, result);
    }
    this->virt_mods = apply_events_to_mods(this->virt_mods, result);
    this->virt_alphas = apply_events_to_alphas(this->virt_alphas, result);
}
