#pragma once
#include "def.h"

#include <cstdint>

typedef u16 ModMask;

enum Mod : u16 {
    NONE = 0,

    LEFT_CTRL = 1 << 0,
    RIGHT_CTRL = 1 << 1,

    LEFT_SHIFT = 1 << 2,
    RIGHT_SHIFT = 1 << 3,

    LEFT_ALT = 1 << 4,
    RIGHT_ALT = 1 << 5,
};

constexpr Mod all_mods[] = {
    LEFT_CTRL,
    RIGHT_CTRL,

    LEFT_SHIFT,
    RIGHT_SHIFT,

    LEFT_ALT,
    RIGHT_ALT,
};
