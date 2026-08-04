#pragma once

#include "kb.h"
#include "window.h"

#include <array>
#include <linux/input.h>
#include <vector>

const size_t KEY_WORDS = KEY_MAX / sizeof(u64) + 1;

using AlphaSet = std::array<u64, KEY_WORDS>; // bitset but uses u64 words for faster traverse, looks like SIMD huh

class KeyMapper {
public:
    KeyMapper();
    void process_evdev_key(const Box<Window>& active_window, const input_event&, std::vector<input_event>& result);

private:
    ModMask phys_mods; // mods held on the physical keyboard
    ModMask virt_mods; // what the OS sees

    AlphaSet phys_alphas;
    AlphaSet virt_alphas;
};
