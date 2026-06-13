#pragma once

#include <format>
#include <iostream>

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"

#define LOG_DEBUG(fmt, ...) std::cerr << "[DEBUG]: " << std::format(fmt __VA_OPT__(, ) __VA_ARGS__) << std::endl

#define LOG_INFO(fmt, ...) std::cerr << "[INFO]: " << std::format(fmt __VA_OPT__(, ) __VA_ARGS__) << std::endl

#define LOG_WARN(fmt, ...)                                                                                             \
    std::cerr << (isatty(STDERR_FILENO) ? COLOR_YELLOW "[WARN]: " COLOR_RESET : "[WARN]: ")                            \
              << std::format(fmt __VA_OPT__(, ) __VA_ARGS__) << std::endl

#define LOG_ERROR(fmt, ...)                                                                                            \
    std::cerr << (isatty(STDERR_FILENO) ? COLOR_RED "[ERROR]: " COLOR_RESET : "[ERROR]: ")                             \
              << std::format(fmt __VA_OPT__(, ) __VA_ARGS__) << std::endl
