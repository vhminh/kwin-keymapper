#pragma once

#include "kb.h"
#include "window.h"

#include <linux/input.h>
#include <vector>

class EvDevInterceptor {
public:
    EvDevInterceptor();
    void process_evdev_key(const Arc<Window>& active_window, const input_event&, std::vector<input_event>& result);

private:
    ModMask phys_mods; // mods held on the physical keyboard
    ModMask virt_mods; // what the OS sees
};
