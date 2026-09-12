#pragma once
#include "CycleClock.hpp"

#include <benchmark/benchmark.h>
#include <array>
#include <print>
#include <type_traits>


class LatencyRecorder {
public:
    LatencyRecorder() : overhead_ticks{CycleClock::instance().getMeasurementOverhead()} {}

    void record(std::uint64_t raw_ticks) {
        if (overhead_ticks > raw_ticks) {
            ++measurement_exceeds_overhead_cost;
            return;
        }
        std::uint64_t ticks = raw_ticks - overhead_ticks;
        if (ticks < bins.size() - 1) {
            ++bins[ticks];
        } else {
            ++bins.back();
            outliers_sum += ticks;
        }
        ++total;
        sum += ticks;
        max_ = std::max(ticks, max_);
    }

    template<typename Callable>
    decltype(auto) record(Callable&& callable) {
        const std::uint32_t epoch = CycleClock::epoch();
        const auto start = CycleClock::begin();

        if constexpr (std::is_same_v<std::result_of_t<Callable()>, void>) {
            callable();
            finish(start, epoch);
        } else {
            decltype(auto) tmp = callable();
            finish(start, epoch);
            return tmp;
        }
    }

    double avgNs() const {
        return total == 0 ? 0.0 : static_cast<double>(sum) / static_cast<double>(total) * nsPerTick();
    }

    double maxNs() const { return static_cast<double>(max_) * nsPerTick(); }

    double percentileNs(double percentile) const {
        if (total == 0) {
            return 0.0;
        }

        if (CycleClock::instance().source() == CycleClock::Source::Rdtsc) {
            return clusteredPercentileNs(percentile);
        }

        const auto target_rank = static_cast<std::uint64_t>(percentile * static_cast<double>(total)) + 1;
        std::uint64_t cumulative{0};
        for (std::size_t bin = 0; bin < bins.size() - 1; ++bin) {
            cumulative += bins[bin];
            if (cumulative >= target_rank) {
                return static_cast<double>(bin) * nsPerTick();
            }
        }
        return bins.back() == 0
                   ? static_cast<double>(bins.size() - 1) * nsPerTick()
                   : static_cast<double>(outliers_sum) / static_cast<double>(bins.back()) * nsPerTick();
    }

    std::uint64_t samples() const { return total; }

    std::uint64_t discarded() const { return discarded_; }

    std::uint64_t measurementExceedsOverhead() const { return measurement_exceeds_overhead_cost; }

private:
    static double nsPerTick() { return CycleClock::instance().nsPerTick(); }

    void finish(std::uint64_t start, std::uint32_t epoch) {
        const std::uint64_t elapsed = CycleClock::end() - start;
        if (CycleClock::epoch() == epoch) [[likely]] {
            record(elapsed);
        } else {
            ++discarded_;
        }
    }

    double clusteredPercentileNs(double percentile) const {
        const auto target_rank = static_cast<std::uint64_t>(percentile * static_cast<double>(total)) + 1;
        std::uint64_t faster_samples{0};
        double previous_center{0.0};
        for (auto cluster = nextClusterFrom(0); cluster.count != 0; cluster = nextClusterFrom(cluster.end)) {
            if (faster_samples + cluster.count < target_rank) {
                faster_samples += cluster.count;
                previous_center = cluster.center;
                continue;
            }

            const double step = faster_samples == 0 ? 0.0 : cluster.center - previous_center;
            const double window_begin = cluster.center - step / 2;
            const double fraction_of_cluster = static_cast<double>(target_rank - faster_samples) / static_cast<double>(cluster.count);
            return (window_begin + fraction_of_cluster * step) * nsPerTick();
        }

        return static_cast<double>(outliers_sum) / static_cast<double>(bins.back()) * nsPerTick();
    }

    constexpr static std::size_t same_grid_point_gap{8};

    struct Cluster {
        double center;
        std::uint64_t count;
        std::size_t end;
    };

    Cluster nextClusterFrom(std::size_t bin) const {
        Cluster cluster{0.0, 0, bins.size() - 1};
        std::uint64_t weighted_sum{0};
        std::size_t last_occupied{0};
        for (; bin < bins.size() - 1; ++bin) {
            if (bins[bin] == 0) {
                if (cluster.count != 0 && bin - last_occupied >= same_grid_point_gap) {
                    break;
                }
                continue;
            }
            cluster.count += bins[bin];
            weighted_sum += bins[bin] * bin;
            last_occupied = bin;
        }
        if (cluster.count != 0) {
            cluster.center = static_cast<double>(weighted_sum) / static_cast<double>(cluster.count);
        }
        cluster.end = bin;
        return cluster;
    }


    constexpr static std::size_t bins_count{1 << 15};

    std::uint64_t overhead_ticks;
    std::array<std::uint64_t, bins_count> bins{};
    std::uint64_t outliers_sum{0};
    std::uint64_t total{0};
    std::uint64_t sum{0};
    std::uint64_t max_{0};
    std::uint64_t measurement_exceeds_overhead_cost{0};
    std::uint64_t discarded_{0};
};

inline void reportLatency(benchmark::State& state, const LatencyRecorder& hist) {
    const auto& clock = CycleClock::instance();
    state.counters["avg_ns"] = benchmark::Counter(hist.avgNs());
    state.counters["p50_ns"] = benchmark::Counter(hist.percentileNs(0.50));
    state.counters["p95_ns"] = benchmark::Counter(hist.percentileNs(0.95));
    state.counters["p99_ns"] = benchmark::Counter(hist.percentileNs(0.99));
    state.counters["p99.9_ns"] = benchmark::Counter(hist.percentileNs(0.999));
    state.counters["max_ns"] = benchmark::Counter(hist.maxNs());
    state.counters["samples"] = benchmark::Counter(static_cast<double>(hist.samples()));
    if (hist.discarded() > 0) {
        state.counters["discarded"] = benchmark::Counter(static_cast<double>(hist.discarded()));
    }
    if (hist.measurementExceedsOverhead() > 0) {
        state.counters["measurement_exceeds_overhead_cost"] = benchmark::Counter(static_cast<double>(hist.measurementExceedsOverhead()));
    }

    state.counters["timer_resolution_ns"] = benchmark::Counter(clock.resolutionNs());
    state.counters["timer_cost_ns"] = benchmark::Counter(static_cast<double>(clock.getMeasurementOverhead()) * clock.nsPerTick());
    if (!clock.countersValid()) {
        state.SkipWithError("performance counter was descheduled mid-run; latency numbers are unreliable");
    }
    const double drift = clock.calculateFrequencyDriftPercent();
    if (std::abs(drift) > 1.0) {
        std::println(stderr, "[CycleClock] WARNING: core frequency moved {:+.1f}% during this benchmark; "
                             "the ns conversion is that far off. Set the governor to performance or re-run the benchmark.", drift);
    }
}


#define RecordOperation(op) \
    if constexpr (record_latency) { \
        latency_recorder.record([&] { return op; }); \
    } \
    else { \
        op; \
    }

#define RecordOperationLambda(...)  \
    if constexpr (record_latency) { \
        latency_recorder.record(__VA_ARGS__); \
    } \
    else { \
        __VA_ARGS__(); \
    }
