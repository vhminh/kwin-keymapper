#include "config.h"

std::tuple<Mod, int> user_process_key(const Window* active_window, Mod mods, int evdev_key) {
    return std::make_tuple(mods, evdev_key);
}
