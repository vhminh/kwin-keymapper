#pragma once

#include "kb.h"
#include "window.h"

#include <linux/input.h>
#include <vector>

class EvDevInterceptor {
public:
    EvDevInterceptor();
    // TODO: return something other than a std::vector to avoid allocation
    std::vector<input_event> process_evdev_key(const Arc<Window>& active_window, const input_event&);

private:
    ModMask phys_mods; // mods held on the physical keyboard
    ModMask virt_mods; // what the OS sees
};
