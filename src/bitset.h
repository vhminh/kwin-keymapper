#pragma once

#include "def.h"

#include <array>
#include <cstddef>

constexpr size_t qwords_size(size_t nbits) {
    return (nbits + 63) / 64;
}

// bitset but uses u64 QWORDs for faster bulk processing, kinda like SIMD huh
template <size_t N>
struct BitSet {
    std::array<u64, qwords_size(N)> qwords{};
    void set(size_t pos, bool value = true) {
        if (value) {
            qwords[pos / 64] |= (u64(1) << (pos % 64));
        } else {
            qwords[pos / 64] &= ~(u64(1) << (pos % 64));
        }
    }
    void unset(size_t pos) { set(pos, false); }
};
