#pragma once

#include "window.h"

#include <memory>

template <typename T>
using Arc = std::shared_ptr<T>;

void user_process_key(Arc<Window> active_window, int key);
