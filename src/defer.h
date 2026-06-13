#pragma once

#include <utility>

template <typename F>
class Defer {
public:
    Defer(F&& f) : f(std::move(f)) {}
    ~Defer() { f(); }

private:
    F f;
};

#define CONCAT_IMPL(x, y) x##y
#define MACRO_CONCAT(x, y) CONCAT_IMPL(x, y)
#define DEFER(code) auto MACRO_CONCAT(deferred_,__COUNTER__) = Defer([&]() { code; });
