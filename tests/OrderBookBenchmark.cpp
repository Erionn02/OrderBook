#include "OrderBook.hpp"
#include "MarketDataGenerator.hpp"
#include "LatencyRecorder.hpp"

#include <benchmark/benchmark.h>
#include <vector>
#include <ranges>

static std::pair<OrderBook, std::vector<OrderId>> buildPopulatedBook(std::size_t n) {
    static std::map<std::size_t, std::vector<Order>> cache{};
    if (!cache.contains(n)) {
        RealisticGenerator gen{};
        cache[n] = gen.generateOrders(n, OrderType::Limit);
    }
    OrderBook book{};
    for (const auto &o: cache[n]) {
        book.addOrder(o);
    }
    auto ids = book.getOrders() | std::views::keys | std::ranges::to<std::vector>();
    std::mt19937_64 rng(n);
    std::ranges::shuffle(ids, rng);
    return {std::move(book), std::move(ids)};
}

template<bool record_latency>
static void BM_AddOrder(benchmark::State &state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    LatencyRecorder latency_recorder;

    MarketParams deep_params{.cancel_rate = 0, .modify_rate = 0};

    RealisticGenerator gen(42, deep_params);
    auto orders = gen.generateOrders(n);

    for (auto _: state) {
        state.PauseTiming();
        OrderBook book{};
        state.ResumeTiming();
        for (const auto &o: orders) {
            RecordOperation(benchmark::DoNotOptimize(book.addOrder(o)))
        }
    }

    if constexpr (record_latency) {
        reportLatency(state, latency_recorder);
    } else {
        state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
    }
}

BENCHMARK(BM_AddOrder<false>)->RangeMultiplier(10)->Range(100'000, 10'000'000)->Unit(benchmark::kSecond)->Name("BM_AddOrder");
BENCHMARK(BM_AddOrder<true>)->Arg(10'000'000)->Unit(benchmark::kSecond)->Name("BM_AddOrderLatency");

template<bool record_latency>
static void BM_CancelOrder(benchmark::State &state) {
    LatencyRecorder latency_recorder;
    const std::size_t n = static_cast<std::size_t>(state.range(0));

    for (auto _: state) {
        state.PauseTiming();
        auto [book, ids] = buildPopulatedBook(n);
        state.ResumeTiming();

        for (OrderId id: ids) {
            RecordOperation(book.cancelOrder(id));
        }
    }

    if constexpr (record_latency) {
        reportLatency(state, latency_recorder);
    } else {
        state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
    }
}

BENCHMARK(BM_CancelOrder<false>)->RangeMultiplier(10)->Range(100'000, 10'000'000)->Unit(benchmark::kSecond)->Name("BM_CancelOrder");
BENCHMARK(BM_CancelOrder<true>)->Arg(10'000'000)->Unit(benchmark::kSecond)->Name("BM_CancelOrderLatency");

template<bool record_latency>
static void BM_ModifyOrder(benchmark::State &state) {
    LatencyRecorder latency_recorder;
    const std::size_t n = static_cast<std::size_t>(state.range(0));

    std::size_t total_processed{0};
    for (auto _: state) {
        state.PauseTiming();
        auto [book, live_ids] = buildPopulatedBook(n);
        total_processed += live_ids.size();
        RealisticGenerator gen{};
        std::vector<std::tuple<OrderId, Quantity, Price> > mods;
        mods.reserve(live_ids.size());
        for (OrderId id: live_ids) {
            Order order = book.getOrder(id);
            mods.emplace_back(id, gen.nextQty(), order.getPrice()); // modify only quantity to avoid filling orders
        }
        state.ResumeTiming();

        for (auto &[id, qty, px]: mods) {
            RecordOperation(book.modifyOrder(id, qty, px));
        }
    }

    if constexpr (record_latency) {
        reportLatency(state, latency_recorder);
    } else {
        state.SetItemsProcessed(static_cast<std::int64_t>(total_processed));
    }
}

BENCHMARK(BM_ModifyOrder<false>)->RangeMultiplier(10)->Range(100'000, 10'000'000)->Unit(benchmark::kSecond)->Name("BM_ModifyOrder");
BENCHMARK(BM_ModifyOrder<true>)->Arg(10'000'000)->Unit(benchmark::kSecond)->Name("BM_ModifyOrderLatency");


template<bool record_latency>
static void BM_MixedStream(benchmark::State &state) {
    LatencyRecorder latency_recorder;
    RealisticGenerator gen{};
    auto events = gen.generateStream(static_cast<std::size_t>(state.range(0)));
    for (auto _: state) {
        OrderBook book{};
        for (const auto &ev: events) {
            // switch turns out to be faster than ev.visit(overloads{...})
            switch (ev.index()) {
                case 0:
                    RecordOperation(benchmark::DoNotOptimize(book.addOrder(std::get<AddEvent>(ev).order)));
                    break;
                case 1:
                    RecordOperation(book.modifyOrder(std::get<ModifyEvent>(ev).targetId, std::get<ModifyEvent>(ev).new_qty, std::get<ModifyEvent>(ev).new_price));
                    break;
                case 2:
                    RecordOperation(book.cancelOrder(std::get<CancelEvent>(ev).targetId));
                    break;
            }
        }
    }

    if constexpr (record_latency) {
        reportLatency(state, latency_recorder);
    } else {
        state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(events.size()));
    }
}

BENCHMARK(BM_MixedStream<false>)->Arg(1'000'000)->Unit(benchmark::kSecond)->Name("BM_MixedStream");
BENCHMARK(BM_MixedStream<true>)->Arg(1'000'000)->Unit(benchmark::kSecond)->Name("BM_MixedStreamLatency");

BENCHMARK_MAIN();
