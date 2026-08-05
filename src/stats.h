#pragma once

#include "def.h"
#include "histogram.h"

#include <array>
#include <string_view>

inline constexpr std::array<u64, 12> buckets{10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120, 10240, 20480};

const int REPORT_INTERVAL = 1024;

class StatsReporter {
public:
    StatsReporter(std::string_view aspect);

    void record(u64 nanos);

private:
    std::string_view aspect;
    Histogram<12> acc_histogram;
    Histogram<12> current_histogram;
    void report();
};
