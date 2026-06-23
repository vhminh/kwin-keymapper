#pragma once

#include "kb.h"
#include "window.h"

#include <tuple>

std::tuple<Mod, int> user_process_key(const Window* active_window, Mod mods, int evdev_key);
