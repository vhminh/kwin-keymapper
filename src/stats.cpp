#include "stats.h"

#include "log.h"

StatsReporter::StatsReporter(std::string_view aspect) :
    aspect(aspect), acc_histogram(buckets), current_histogram(buckets) {}

void StatsReporter::record(u64 nanos) {
    this->acc_histogram.record(nanos);
    this->current_histogram.record(nanos);
    if (this->current_histogram.total() >= REPORT_INTERVAL) {
        this->report();
        // reset
        this->current_histogram = Histogram<12>(buckets);
    }
}

void StatsReporter::report() {
    LOG_INFO(
        "stats for \"{}\" last report: total={} invocations, min={}ns, p50={}ns, p90={}ns, p95={}ns, p99={}ns, "
        "max={}ns",
        aspect,
        this->current_histogram.total(),
        this->current_histogram.min(),
        this->current_histogram.quantile(0.5),
        this->current_histogram.quantile(0.9),
        this->current_histogram.quantile(0.95),
        this->current_histogram.quantile(0.99),
        this->current_histogram.max()
    );
    LOG_INFO(
        "accumulated stats for \"{}\": total={} invocations, min={}ns, p50={}ns, p90={}ns, p95={}ns, p99={}ns, "
        "max={}ns",
        aspect,
        this->acc_histogram.total(),
        this->acc_histogram.min(),
        this->acc_histogram.quantile(0.5),
        this->acc_histogram.quantile(0.9),
        this->acc_histogram.quantile(0.95),
        this->acc_histogram.quantile(0.99),
        this->acc_histogram.max()
    );
}
