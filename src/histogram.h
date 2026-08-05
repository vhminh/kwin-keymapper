#pragma once

#include "def.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <format>

struct Quantile {
    double value;
    bool inf_bucket;
};
template <>
struct std::formatter<Quantile> : std::formatter<std::string> {
    auto format(const Quantile& q, std::format_context& ctx) const {
        if (q.inf_bucket) {
            return std::formatter<std::string>::format(std::format(">{}", q.value), ctx);
        } else {
            return std::formatter<std::string>::format(std::format("{}", q.value), ctx);
        }
    }
};

// includes +Inf bucket with "bound" tail
template <size_t N>
class Histogram {
    static_assert(N > 2, "expect more than 2 buckets");

public:
    Histogram(std::array<u64, N> buckets) : buckets(buckets), counts{} {
        for (size_t i = 0; i < buckets.size() - 1; ++i) {
            assert(buckets[i] < buckets[i + 1]);
        }
    }

    void record(u64 value) {
        size_t idx = std::upper_bound(buckets.begin(), buckets.end(), value) - buckets.begin();
        counts[idx]++;
        _min = std::min(_min, value);
        _max = std::max(_max, value);
        ++_total;
    }

    u64 min() { return _min; }
    u64 max() { return _max; }
    u64 total() { return _total; }

    Quantile quantile(double q) {
        if (_total == 0)
            return {NAN, false};
        q = std::clamp(q, 0.0, 1.0);

        double target = q * _total;
        double cum = 0.0;
        for (size_t i = 0; i < counts.size(); ++i) {
            if (counts[i] > 0 && cum + counts[i] >= target) {
                double low = (i == 0) ? 0.0 : buckets[i - 1];
                return {std::lerp(low, buckets[i], (target - cum) / counts[i]), false};
            }
            cum += counts[i];
        }
        return {double(buckets.back()), true}; // Inf
    }

private:
    std::array<u64, N> buckets;
    std::array<u64, N + 1> counts;
    u64 _total = 0;
    u64 _min = -1;
    u64 _max = 0;
};
