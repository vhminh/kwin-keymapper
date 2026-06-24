#pragma once

#include "def.h"
#include "kb.h"
#include "window.h"

#include <tuple>

std::tuple<ModMask, u16> user_key_map(const Arc<Window>& active_window, ModMask mods, u16 evdev_key);
