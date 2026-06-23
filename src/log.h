#pragma once

#include <cstdlib>
#include <format>
#include <iostream>

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"

#define LOG_IMPL(level, color, fmt, ...)                                                                               \
    std::cerr << std::format(color "[" level "]: " COLOR_RESET fmt "\n" __VA_OPT__(, ) __VA_ARGS__)

#define LOG_DEBUG(fmt, ...) LOG_IMPL("DEBUG", COLOR_RESET, fmt __VA_OPT__(, ) __VA_ARGS__)

#define LOG_INFO(fmt, ...) LOG_IMPL("INFO", COLOR_RESET, fmt __VA_OPT__(, ) __VA_ARGS__)

#define LOG_WARN(fmt, ...)                                                                                             \
    isatty(STDERR_FILENO) ? LOG_IMPL("WARN", COLOR_YELLOW, fmt __VA_OPT__(, ) __VA_ARGS__) :                           \
                            LOG_IMPL("WARN", COLOR_RESET, fm __VA_OPT__(, ) __VA_ARGS__)

#define LOG_ERROR(fmt, ...)                                                                                            \
    isatty(STDERR_FILENO) ? LOG_IMPL("ERROR", COLOR_RED, fmt __VA_OPT__(, ) __VA_ARGS__) :                             \
                            LOG_IMPL("ERROR", COLOR_RESET, fmt __VA_OPT__(, ) __VA_ARGS__)

#define TODO(fmt, ...)                                                                                                 \
    do {                                                                                                               \
        LOG_ERROR("TODO(" __FILE__ ":{}): " fmt, __LINE__ __VA_OPT__(, ) __VA_ARGS__);                                \
        std::abort();                                                                                                  \
    } while (0)
