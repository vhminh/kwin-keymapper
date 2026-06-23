#pragma once

#include "kb.h"
#include "window.h"

#include <memory>
#include <tuple>

template <typename T>
using Arc = std::shared_ptr<T>;

std::tuple<Mod, int> user_key_map(const Arc<Window>& active_window, Mod mods, int evdev_key);
