#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <print>
#include <vector>
#include <benchmark/benchmark.h>
#include <x86intrin.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>


class CycleClock {
public:
    enum class Source { Rdpmc, Rdtsc };

    static CycleClock& instance() {
        static CycleClock clock{};
        return clock;
    }

    [[gnu::always_inline]] static inline std::uint64_t begin() noexcept {
        const std::uint64_t value = read();
        _mm_lfence();
        return value;
    }

    [[gnu::always_inline]] static inline std::uint64_t end() noexcept {
        _mm_lfence();
        return read();
    }

    [[gnu::always_inline]] static inline std::uint32_t epoch() noexcept {
        if (page == nullptr) [[unlikely]] {
            return 0;
        }
        std::atomic_signal_fence(std::memory_order_seq_cst);
        const std::uint32_t seq = __atomic_load_n(&page->lock, __ATOMIC_RELAXED);
        std::atomic_signal_fence(std::memory_order_seq_cst);
        return seq;
    }

    std::uint64_t getMeasurementOverhead() const { return overhead_ticks; }

    double nsPerTick() const { return ns_per_tick; }

    double calculateFrequencyDriftPercent() const {
        const double now = calibrateNsPerTick();
        return (now - ns_per_tick) / ns_per_tick * 100.0;
    }

    Source source() const { return source_; }

    const char* sourceName() const { return source_ == Source::Rdpmc ? "rdpmc" : "rdtsc"; }

    double resolutionNs() const { return source_ == Source::Rdpmc ? ns_per_tick : static_cast<double>(tsc_step) * ns_per_tick; }

    bool countersValid() const {
        return source_ != Source::Rdpmc || (page != nullptr && page->index == rdpmc_index + 1);
    }

private:
    CycleClock() {
        openCounter();
        ns_per_tick = calibrateNsPerTick();
        if (source_ == Source::Rdtsc) {
            tsc_step = measureTscStep();
        }
        overhead_ticks = calibrateOverhead();
        if (source_ == Source::Rdtsc) {
            std::println(stderr,"[CycleClock] WARNING: falling back to RDTSC (resolution {:.1f} ns).", static_cast<double>(tsc_step) * ns_per_tick);
        }
    }

    [[gnu::always_inline]] static inline std::uint64_t read() noexcept {
        if (rdpmc_index != rdpmc_no_index) [[likely]] {
            return __rdpmc(static_cast<int>(rdpmc_index)) & counter_mask;
        }
        return __rdtsc();
    }

    void openCounter() {
        perf_event_attr attr{};
        attr.type = PERF_TYPE_HARDWARE;
        attr.size = sizeof(attr);
        attr.config = PERF_COUNT_HW_CPU_CYCLES;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        attr.pinned = 1;

        fd = static_cast<int>(syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0));
        if (fd < 0) {
            std::println(stderr, "[CycleClock] perf_event_open failed: {}", std::strerror(errno));
            return;
        }

        void* mapped = mmap(nullptr, pageSize, PROT_READ, MAP_SHARED, fd, 0);
        if (mapped == MAP_FAILED) {
            std::println(stderr, "[CycleClock] mmap of perf page failed: {}", std::strerror(errno));
            close(fd);
            fd = -1;
            return;
        }

        page = static_cast<perf_event_mmap_page*>(mapped);
        if (page->cap_user_rdpmc == 0 || page->index == 0) {
            std::println(stderr, "[CycleClock] userspace rdpmc unavailable (cap_user_rdpmc={} index={})", static_cast<unsigned>(page->cap_user_rdpmc), static_cast<unsigned>(page->index));
            return;
        }

        counter_mask = page->pmc_width == 0 || page->pmc_width >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << page->pmc_width) - 1;
        rdpmc_index = page->index - 1;
        source_ = Source::Rdpmc;
    }

    static double calibrateNsPerTick() {
        const auto warmup_begin = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - warmup_begin < std::chrono::milliseconds(30)) {}

        const auto wall_begin = std::chrono::steady_clock::now();
        const std::uint64_t ticks_begin = begin();
        while (std::chrono::steady_clock::now() - wall_begin < std::chrono::milliseconds(50)) {}
        const std::uint64_t ticks_end = end();
        const auto wall_end = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(wall_end - wall_begin).count();
        return ns / static_cast<double>(ticks_end - ticks_begin);
    }

#define CHAIN_ADD8   "add $1,%0\n\tadd $1,%0\n\tadd $1,%0\n\tadd $1,%0\n\tadd $1,%0\n\tadd $1,%0\n\tadd $1,%0\n\tadd $1,%0\n\t"
#define CHAIN_ADD64  CHAIN_ADD8 CHAIN_ADD8 CHAIN_ADD8 CHAIN_ADD8 CHAIN_ADD8 CHAIN_ADD8 CHAIN_ADD8 CHAIN_ADD8
#define CHAIN_ADD512 CHAIN_ADD64 CHAIN_ADD64 CHAIN_ADD64 CHAIN_ADD64 CHAIN_ADD64 CHAIN_ADD64 CHAIN_ADD64 CHAIN_ADD64

    std::uint64_t calibrateOverhead() const {
        static constexpr std::size_t CALIBRATE_MAX_ATTEMPTS{10};
        std::uint64_t overhead{0};
        for (std::size_t i{0}; i < CALIBRATE_MAX_ATTEMPTS; ++i) {
            auto epoch_beg = epoch();
            overhead = calibrateOverheadInternal();
            if (epoch_beg == epoch() && overhead != 0) {
                return overhead;
            }
        }
        std::println("[CycleClock] WARNING: Couldn't calculate precise measurement overhead.");
        return overhead;
    }

    std::uint64_t calibrateOverheadInternal() const {
        const double shorter = trimmedMeanTicks([](std::uint64_t& x) {
            __asm__ volatile(CHAIN_ADD512 : "+r"(x));
        });
        const double longer = trimmedMeanTicks([](std::uint64_t& x) {
            __asm__ volatile(CHAIN_ADD512 : "+r"(x));
            __asm__ volatile(CHAIN_ADD512 : "+r"(x));
        });
        if (longer <= shorter) {
            return 0;
        }
        constexpr double chain_cycles = 512.0;
        const double ticks_per_cycle = (longer - shorter) / chain_cycles;
        const double overhead = shorter - chain_cycles * ticks_per_cycle;
        return overhead > 0.0 ? static_cast<std::uint64_t>(overhead + 0.5) : 0;
    }

    static double trimmedMeanTicks(auto&& body) {
        std::vector<std::uint64_t> measured = sampleTicks(body);
        std::ranges::sort(measured);
        const std::size_t kept = measured.size() - measured.size() / 100;
        double total = 0.0;
        for (std::size_t i = 0; i < kept; ++i) {
            total += static_cast<double>(measured[i]);
        }
        return total / static_cast<double>(kept);
    }

    static std::vector<std::uint64_t> sampleTicks(auto&& body) {
        constexpr std::size_t samples = 20'000;
        std::vector<std::uint64_t> measured(samples);
        std::uint64_t x = 1;
        for (std::size_t i = 0; i < samples; ++i) {
            benchmark::DoNotOptimize(x);
            const std::uint64_t a = begin();
            body(x);
            const std::uint64_t b = end();
            measured[i] = b - a;
        }
        benchmark::DoNotOptimize(x);
        return measured;
    }

    static std::uint64_t measureTscStep() {
        std::vector<std::uint64_t> steps;
        steps.reserve(4096);
        std::uint64_t previous = __rdtsc();
        for (std::size_t i = 0; i < 200'000 && steps.size() < 4096; ++i) {
            const std::uint64_t now = __rdtsc();
            if (now != previous) {
                steps.push_back(now - previous);
                previous = now;
            }
        }
        if (steps.empty()) {
            return 1;
        }
        std::ranges::nth_element(steps, steps.begin() + static_cast<std::ptrdiff_t>(steps.size() / 2));
        return std::max<std::uint64_t>(steps[steps.size() / 2], 1);
    }

    static constexpr std::uint32_t rdpmc_no_index{~std::uint32_t{0}};
    static constexpr std::size_t pageSize{4096};

    static inline std::uint32_t rdpmc_index{rdpmc_no_index};
    static inline std::uint64_t counter_mask{~std::uint64_t{0}};
    static inline perf_event_mmap_page* page{nullptr};

    int fd{-1};
    Source source_{Source::Rdtsc};
    double ns_per_tick{0.0};
    std::uint64_t overhead_ticks{0};
    std::uint64_t tsc_step{1};
};
