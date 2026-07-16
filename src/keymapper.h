#pragma once

#include "kb.h"
#include "window.h"

#include <linux/input.h>
#include <vector>

class KeyMapper {
public:
    KeyMapper();
    void process_evdev_key(const Box<Window>& active_window, const input_event&, std::vector<input_event>& result);

private:
    ModMask phys_mods; // mods held on the physical keyboard
    ModMask virt_mods; // what the OS sees
};
