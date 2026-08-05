#include "keymapper.h"

#include "config.h"
#include "log.h"

#include <bit>
#include <cassert>
#include <linux/input.h>

// reset lowest set bit
inline u64 blsr(u64 value) {
    return value & (value - 1);
}

KeyMapper::KeyMapper() : phys_mods(0), virt_mods(0), phys_non_mods{}, virt_non_mods{} {}

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

KeyMask single_alpha_key_set(u16 key) {
    KeyMask mask;
    mask.set(key);
    return mask;
}

void produce_mod_diff(ModMask current, ModMask expected, timeval time, std::vector<input_event>& res) {
    for (Mod m : all_mods) {
        if ((current & m) && ((expected & m) == 0)) {
            res.push_back(input_event{.time = time, .type = EV_KEY, .code = mod_to_evdev(m), .value = 0});
        }
    }
    for (Mod m : all_mods) {
        if (((current & m) == 0) && (expected & m)) {
            res.push_back(input_event{.time = time, .type = EV_KEY, .code = mod_to_evdev(m), .value = 1});
        }
    }
}

void produce_non_mod_diff(
    const KeyMask& current, const KeyMask& expected, timeval time, std::vector<input_event>& res
) {
    auto emit_bits = [](u64 bits, size_t base, timeval time, i32 pressed, std::vector<input_event>& res) {
        for (; bits; bits = blsr(bits)) {
            u16 code = static_cast<u16>(base + std::countr_zero(bits));
            res.push_back(input_event{.time = time, .type = EV_KEY, .code = code, .value = pressed});
        }
    };
    for (size_t i = 0; i < current.qwords.size(); ++i) {
        u64 releases = (current.qwords[i] & ~expected.qwords[i]);
        if (releases != 0) {
            emit_bits(releases, i * 64, time, 0, res);
        }
    }
    for (size_t i = 0; i < current.qwords.size(); ++i) {
        u64 presses = (expected.qwords[i] & ~current.qwords[i]);
        if (presses != 0) {
            emit_bits(presses, i * 64, time, 1, res);
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

void apply_event_to_non_mods(KeyMask& mask, const input_event& ev) {
    if (is_evdev_mod(ev.code)) {
        return;
    }
    if (ev.value == 0) {
        // released
        mask.unset(ev.code);
    } else if (ev.value == 1) {
        // pressed
        mask.set(ev.code);
    }
}

void apply_events_to_non_mods(KeyMask& mask, const std::vector<input_event>& events) {
    for (const input_event& ev : events) {
        apply_event_to_non_mods(mask, ev);
    }
}

void KeyMapper::process_evdev_key(
    const Box<Window>& active_window, const input_event& ev, std::vector<input_event>& result
) {
    this->phys_mods = apply_event_to_mods(this->phys_mods, ev);
    apply_event_to_non_mods(this->phys_non_mods, ev);

    bool new_alpha_key_pressed = !is_evdev_mod(ev.code) && (ev.value == 1 || ev.value == 2);
    ModMask expected_virt_mods = this->phys_mods;
    u16 expected_virt_key = ev.code;
    if (new_alpha_key_pressed) {
        std::tie(expected_virt_mods, expected_virt_key) = user_key_map(active_window, phys_mods, ev.code);
    }

    bool should_remap = expected_virt_mods != this->phys_mods || expected_virt_key != ev.code;
    if (should_remap) {
        // make the virt state matches the user defined mapping
        produce_mod_diff(this->virt_mods, expected_virt_mods, ev.time, result);
        produce_non_mod_diff(this->virt_non_mods, single_alpha_key_set(expected_virt_key), ev.time, result);
    } else {
        // make the virt state matches the phys state
        produce_mod_diff(this->virt_mods, this->phys_mods, ev.time, result);
        produce_non_mod_diff(this->virt_non_mods, this->phys_non_mods, ev.time, result);
    }

    this->virt_mods = apply_events_to_mods(this->virt_mods, result);
    apply_events_to_non_mods(this->virt_non_mods, result);
}

#include "test.h"
input_event down(u16 code) {
    return input_event{.time = {}, .type = EV_KEY, .code = code, .value = 1};
}

input_event up(u16 code) {
    return input_event{.time = {}, .type = EV_KEY, .code = code, .value = 0};
}

void assert_keys(
    KeyMapper& mapper, const Box<Window>& active_window, input_event input,
    const std::vector<input_event>& expected_outputs
) {
    std::vector<input_event> outputs;
    mapper.process_evdev_key(active_window, input, outputs);
    LOG_INFO("outputs:");
    for (auto ev : outputs) {
        LOG_INFO("type: {}, code: {}, value: {}", ev.type, ev.code, ev.value);
    }
    LOG_INFO("expected outputs:");
    for (auto ev : expected_outputs) {
        LOG_INFO("type: {}, code: {}, value: {}", ev.type, ev.code, ev.value);
    }
    ASSERT_EQ(outputs.size(), expected_outputs.size());
    for (size_t i = 0; i < outputs.size(); ++i) {
        ASSERT_EQ(outputs[i].type, expected_outputs[i].type);
        ASSERT_EQ(outputs[i].code, expected_outputs[i].code);
        ASSERT_EQ(outputs[i].value, expected_outputs[i].value);
    }
}

static Box<Window> firefox = std::make_unique<Window>("firefox", "firefox", "Youtube");
static Box<Window> alacritty = std::make_unique<Window>("Alacritty", "alacritty", "Alacritty - kwin-keymapper");

TEST_CASE("maps Alt+C to Ctrl+C in GUI apps") {
    KeyMapper mapper;
    assert_keys(mapper, firefox, down(KEY_LEFTALT), {down(KEY_LEFTALT)});
    assert_keys(mapper, firefox, down(KEY_C), {up(KEY_LEFTALT), down(KEY_LEFTCTRL), down(KEY_C)});
    assert_keys(mapper, firefox, up(KEY_C), {up(KEY_LEFTCTRL), down(KEY_LEFTALT), up(KEY_C)});
    assert_keys(mapper, firefox, up(KEY_LEFTALT), {up(KEY_LEFTALT)});
}

TEST_CASE("maps Alt+C to Ctrl+Shift+C in terminal apps") {
    KeyMapper mapper;
    assert_keys(mapper, alacritty, down(KEY_LEFTALT), {down(KEY_LEFTALT)});
    assert_keys(
        mapper, alacritty, down(KEY_C), {up(KEY_LEFTALT), down(KEY_LEFTCTRL), down(KEY_LEFTSHIFT), down(KEY_C)}
    );
    assert_keys(mapper, alacritty, up(KEY_C), {up(KEY_LEFTCTRL), up(KEY_LEFTSHIFT), down(KEY_LEFTALT), up(KEY_C)});
    assert_keys(mapper, alacritty, up(KEY_LEFTALT), {up(KEY_LEFTALT)});
}

TEST_CASE("maps Alt+Shift+F to Ctrl+Shift+F in GUI apps") {
    KeyMapper mapper;
    assert_keys(mapper, firefox, down(KEY_LEFTSHIFT), {down(KEY_LEFTSHIFT)});
    assert_keys(mapper, firefox, down(KEY_LEFTALT), {down(KEY_LEFTALT)});
    assert_keys(mapper, firefox, down(KEY_F), {up(KEY_LEFTALT), down(KEY_LEFTCTRL), down(KEY_F)});
    assert_keys(mapper, firefox, up(KEY_F), {up(KEY_LEFTCTRL), down(KEY_LEFTALT), up(KEY_F)});
    assert_keys(mapper, firefox, up(KEY_LEFTALT), {up(KEY_LEFTALT)});
    assert_keys(mapper, firefox, up(KEY_LEFTSHIFT), {up(KEY_LEFTSHIFT)});
}

TEST_CASE("maps Ctrl+N/Ctrl+P to Up/Down in GUI apps") {
    KeyMapper mapper;
    assert_keys(mapper, firefox, down(KEY_LEFTCTRL), {down(KEY_LEFTCTRL)});
    assert_keys(mapper, firefox, down(KEY_N), {up(KEY_LEFTCTRL), down(KEY_DOWN)});
    assert_keys(mapper, firefox, down(KEY_P), {up(KEY_DOWN), down(KEY_UP)});
    assert_keys(mapper, firefox, up(KEY_N), {down(KEY_LEFTCTRL), up(KEY_UP), down(KEY_P)});
    assert_keys(mapper, firefox, up(KEY_P), {up(KEY_P)});
    assert_keys(mapper, firefox, up(KEY_LEFTCTRL), {up(KEY_LEFTCTRL)});
}
