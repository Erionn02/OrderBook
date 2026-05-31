#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <cmath>

#include "OrderBook.hpp"


class NaiveMarketDataGenerator {
    std::mt19937 rng;
    std::normal_distribution<double> price_dist;
    std::exponential_distribution<double> qty_dist;
    std::uniform_int_distribution<int> side_dist;

    Price current_mid_price = 10000;
    OrderId id = 1;

public:
    NaiveMarketDataGenerator(uint32_t seed = 42)
        : rng(seed),
          price_dist(0.0, 5.0),
          qty_dist(0.05),
          side_dist(0, 1) {}

    Order generateNextOrder() {
        TradeSide side = (side_dist(rng) == 0) ? TradeSide::Buy : TradeSide::Sell;

        auto price_offset = static_cast<Price>(std::round(price_dist(rng)));
        Price order_price = current_mid_price + price_offset;

        Quantity qty = std::max<Quantity>(1, static_cast<Quantity>(std::round(qty_dist(rng))));

        if (price_offset > 3) current_mid_price += 1;
        if (price_offset < -3) current_mid_price -= 1;

        return {
            id++,
            OrderType::Limit,
            qty,
            order_price,
            side
        };
    }
};


static void BM_OrderBook_AddOrder(benchmark::State& state) {
    OrderBook book{};
    NaiveMarketDataGenerator generator(42);
    std::vector<Order> pre_filled_orders{};
    for (size_t i = 0; i < state.range_size(); ++i) {
        pre_filled_orders.push_back(generator.generateNextOrder());
    }
    std::size_t i{0};
    for (auto _ : state) {
        benchmark::DoNotOptimize(book.addOrder(pre_filled_orders[i++]));
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_OrderBook_AddOrder)
    ->RangeMultiplier(10)
    ->Range(10, 1'000'000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();