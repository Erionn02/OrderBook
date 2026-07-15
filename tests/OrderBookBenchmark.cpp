#include "OrderBook.hpp"
#include "MarketDataGenerator.hpp"
#include "LatencyRecorder.hpp"

#include <benchmark/benchmark.h>
#include <vector>
#include <ranges>

static std::tuple<OrderBook, std::vector<Order>, std::vector<OrderId>> buildPopulatedBook(std::size_t n, RealisticGenerator& gen) {
    static std::map<std::size_t, std::vector<Order>> cache{};
    if (!cache.contains(n)) {
        cache[n] = gen.generateOrders(n, OrderType::Limit);
    }
    OrderBook book{};
    for (const auto &o: cache[n]) {
        book.addOrder(o);
    }
    auto ids = book.getOrders() | std::views::keys | std::ranges::to<std::vector>();
    std::mt19937_64 rng(n);
    std::ranges::shuffle(ids, rng);
    auto live_orders = book.getOrders() | std::views::values | std::views::keys | std::views::transform([](auto&& it) -> decltype(auto) {
                           return *it;
                       })
                       | std::ranges::to<std::vector>();
    return {std::move(book), live_orders, std::move(ids)};
}

static std::tuple<OrderBook, std::vector<Order>, std::vector<OrderId>> buildPopulatedBook(std::size_t n) {
    RealisticGenerator gen{};
    return buildPopulatedBook(n, gen);
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
        auto [book, _, ids] = buildPopulatedBook(n);
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
        auto [book, _, live_ids] = buildPopulatedBook(n);
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
            if (auto event = std::get_if<AddEvent>(&ev)) {
                RecordOperation(benchmark::DoNotOptimize(book.addOrder(event->order)));
            } else if (auto event = std::get_if<ModifyEvent>(&ev)) {
                RecordOperation(book.modifyOrder(event->targetId, event->new_qty, event->new_price));
            } else {
                RecordOperation(book.cancelOrder(std::get<CancelEvent>(ev).targetId));
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


template<bool record_latency>
static void BM_ReplaceOrder(benchmark::State &state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    LatencyRecorder latency_recorder;
    MarketParams deep_params{.cancel_rate = 0, .modify_rate = 0};

    for (auto _: state) {
        state.PauseTiming();

        RealisticGenerator gen(42, deep_params);
        auto [book, live_orders, _] = buildPopulatedBook(n, gen);
        auto new_orders = gen.generateOrders(live_orders.size());
        state.ResumeTiming();
        for (const auto &[old_order, new_order]: std::views::zip(live_orders, new_orders)) {
            RecordOperation(benchmark::DoNotOptimize(book.replaceOrder(old_order.getId(), new_order.getId(), new_order.getQuantity(), new_order.getPrice())));
        }
    }

    if constexpr (record_latency) {
        reportLatency(state, latency_recorder);
    } else {
        state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
    }
}

BENCHMARK(BM_ReplaceOrder<false>)->RangeMultiplier(10)->Range(100'000, 10'000'000)->Unit(benchmark::kSecond)->Name("BM_ReplaceOrder");
BENCHMARK(BM_ReplaceOrder<true>)->Arg(10'000'000)->Unit(benchmark::kSecond)->Name("BM_ReplaceOrderLatency");

template<bool record_latency>
static void BM_ReduceExecutedOrder(benchmark::State &state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    LatencyRecorder latency_recorder;
    MarketParams deep_params{.cancel_rate = 0, .modify_rate = 0};

    std::mt19937_64 rng{2137};
    double order_fills_after_reduction_probability{0.3};
    std::bernoulli_distribution distribution{order_fills_after_reduction_probability};
    auto shouldOrderFill = [&] { return distribution(rng); };
    std::int64_t total_processed{0};
    for (auto _: state) {
        state.PauseTiming();
        auto [book, live_orders, _] = buildPopulatedBook(n);

        total_processed += static_cast<std::int64_t>(live_orders.size());
        std::vector<std::pair<OrderId, Quantity>> reductions{};
        reductions.reserve(live_orders.size());
        for (auto order : live_orders) {
            if (shouldOrderFill()) {
                reductions.emplace_back(order.getId(), order.getQuantity());
            } else {
                reductions.emplace_back(order.getId(), order.getQuantity() / 2);
            }
        }

        state.ResumeTiming();
        for (const auto &[order_id, reduced_quantity]: reductions) {
            RecordOperation(book.reduceExecutedOrder(order_id, reduced_quantity));
        }
    }

    if constexpr (record_latency) {
        reportLatency(state, latency_recorder);
    } else {
        state.SetItemsProcessed(total_processed);
    }
}

BENCHMARK(BM_ReduceExecutedOrder<false>)->RangeMultiplier(10)->Range(100'000, 10'000'000)->Unit(benchmark::kSecond)->Name("BM_ReduceExecutedOrder");
BENCHMARK(BM_ReduceExecutedOrder<true>)->Arg(10'000'000)->Unit(benchmark::kSecond)->Name("BM_ReduceExecutedOrderLatency");

BENCHMARK_MAIN();
