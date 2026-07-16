#pragma once
#include <cstdint>
#include <memory>

template <typename T>
using Box = std::unique_ptr<T>;

template <typename T>
using Arc = std::shared_ptr<T>;

using u16 = std::uint16_t;
