#pragma once

#include "kb.h"
#include "window.h"

#include <linux/input.h>
#include <vector>

class EvDevInterceptor {
public:
    EvDevInterceptor();
    std::vector<input_event> process_evdev_key(const Window* active_window, const input_event&);

private:
    Mod held_mods;
};
