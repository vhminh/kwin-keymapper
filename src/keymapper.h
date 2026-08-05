#pragma once

#include "bitset.h"
#include "kb.h"
#include "window.h"

#include <linux/input.h>
#include <vector>

using KeyMask = BitSet<KEY_MAX + 1>;

class KeyMapper {
public:
    KeyMapper();
    void process_evdev_key(const Box<Window>& active_window, const input_event&, std::vector<input_event>& result);

private:
    ModMask phys_mods; // mods held on the physical keyboard
    ModMask virt_mods; // what the OS sees

    KeyMask phys_non_mods;
    KeyMask virt_non_mods;
};
