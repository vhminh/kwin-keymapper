#pragma once

#include "window.h"

#include <cstdint>

enum Mod : uint32_t {
    LEFT_CTRL = 1 << 0,
    RIGHT_CTRL = 1 << 1,

    LEFT_SHIFT = 1 << 2,
    RIGHT_SHIFT = 1 << 3,

    LEFT_ALT = 1 << 4,
    RIGHT_ALT = 1 << 5,
};

void user_process_key(const Window* active_window, Mod mods, int evdev_key);
