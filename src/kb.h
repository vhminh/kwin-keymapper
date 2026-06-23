#pragma once
#include <cstdint>

typedef uint32_t ModMask;

enum Mod : uint32_t {
    NONE = 0,

    LEFT_CTRL = 1 << 0,
    RIGHT_CTRL = 1 << 1,

    LEFT_SHIFT = 1 << 2,
    RIGHT_SHIFT = 1 << 3,

    LEFT_ALT = 1 << 4,
    RIGHT_ALT = 1 << 5,
};
