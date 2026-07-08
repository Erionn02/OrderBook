#pragma once
#include <benchmark/benchmark.h>

#include <array>
#include <chrono>
#include <x86intrin.h>


inline double nanosecondsPerCycle() {
    static const double ns_per_cycle = [] {
        const auto t0 = std::chrono::steady_clock::now();
        const std::uint64_t c0 = __rdtsc();
        while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(20)) {
        }
        const std::uint64_t c1 = __rdtsc();
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(c1 - c0);
    }();
    return ns_per_cycle;
}

class LatencyRecorder {
public:
    void record(std::uint64_t cycles) {
        if (cycles < bins.size() - 1) {
            ++bins[cycles];
        } else {
            ++bins.back();
            outliers_sum += cycles;
        }
        ++total_;
        sum += cycles;
        max_ = std::max(cycles, max_);
    }

    template<typename Callable>
    decltype(auto) record(Callable && callable) {
        const auto start = cpuCyclesBegin();
        if constexpr (std::is_same_v<std::result_of_t<Callable()>, void>) {
            callable();
            record(cpuCyclesEnd() - start);
        } else {
            decltype(auto) tmp = callable();
            record(cpuCyclesEnd() - start);
            return tmp;
        }
    }

    double avgNs() const {
        return total_ == 0 ? 0.0 : static_cast<double>(sum) / static_cast<double>(total_) * nanosecondsPerCycle();
    }

    double maxNs() const {
        return static_cast<double>(max_) * nanosecondsPerCycle();
    }

    double percentileNs(double percentile) const {
        if (total_ == 0) {
            return 0.0;
        }
        const auto target_rank = static_cast<std::uint64_t>(percentile * static_cast<double>(total_)) + 1;
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
            return (window_begin + fraction_of_cluster * step) * nanosecondsPerCycle();
        }

        return static_cast<double>(outliers_sum) / static_cast<double>(bins.back()) * nanosecondsPerCycle();
    }

    std::uint64_t samples() const {
        return total_;
    }

private:
    std::uint64_t cpuCyclesBegin() {
        return __rdtsc();
    }

    std::uint64_t cpuCyclesEnd() {
        unsigned aux;
        return __rdtscp(&aux);
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
    std::array<std::uint64_t, bins_count> bins{};
    std::uint64_t outliers_sum{0};
    std::uint64_t total_{0};
    std::uint64_t sum{0};
    std::uint64_t max_{0};
};

inline void reportLatency(benchmark::State& state, const LatencyRecorder& hist) {
    state.counters["avg_ns"] = benchmark::Counter(hist.avgNs());
    state.counters["p50_ns"] = benchmark::Counter(hist.percentileNs(0.50));
    state.counters["p95_ns"] = benchmark::Counter(hist.percentileNs(0.95));
    state.counters["p99_ns"] = benchmark::Counter(hist.percentileNs(0.99));
    state.counters["p99.9_ns"] = benchmark::Counter(hist.percentileNs(0.999));
    state.counters["max_ns"] = benchmark::Counter(hist.maxNs());
    state.counters["samples"] = benchmark::Counter(static_cast<double>(hist.samples()));
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
