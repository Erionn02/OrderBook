#pragma once
#include "Order.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <meta>
#include <inplace_vector>
#include <variant>



struct MarketParams {
    double initial_mid_price = 10'000.00;
    double tick_size = 1.0;

    double price_reversion = 0.05;
    double price_volatility = 0.002;

    double spread_mean_ticks = 2.0;
    double spread_std_ticks = 0.8;

    double quantity_alpha = 1.5;
    double quantity_min = 1.0;
    double quantity_max = 10'000.0;

    double buy_bias = 0.50;

    double cancel_rate = 0.20;
    double modify_rate = 0.10;

    using OrderTypeProbability = std::pair<OrderType, double>;
    std::inplace_vector<OrderTypeProbability, std::meta::enumerators_of(^^OrderType).size()> orderTypeProbability{
        {OrderType::Limit, 0.95},
        {OrderType::FillOrKill, 0.05}
    };
};

enum class EventKind : std::uint8_t { Add, Cancel, Modify };

struct AddEvent {
    Order order;
};

struct ModifyEvent {
    OrderId targetId;
    Quantity new_qty;
    Price new_price;
};

struct CancelEvent {
    OrderId targetId;
};

using BenchEvent = std::variant<AddEvent, ModifyEvent, CancelEvent>;


class RealisticGenerator {
public:
    explicit RealisticGenerator(std::uint64_t seed = 42,
                                MarketParams p = {})
        : rng(seed), rng_double(seed), params(p), mid(p.initial_mid_price) {
        std::ranges::sort(params.orderTypeProbability, std::greater<>{}, &MarketParams::OrderTypeProbability::second);
    }

    Price nextMid() {
        double log_mid = std::log(mid);
        double target = std::log(params.initial_mid_price);
        log_mid += params.price_reversion * (target - log_mid)
                + params.price_volatility * std_norm(rng);
        mid = std::exp(log_mid);
        return static_cast<Price>(
            std::round(mid / params.tick_size) * params.tick_size);
    }

    Price halfSpread() {
        std::normal_distribution<double> d(params.spread_mean_ticks,
                                           params.spread_std_ticks);
        double ticks = std::max(1.0, std::round(d(rng)));
        return static_cast<Price>(ticks * params.tick_size);
    }

    Quantity nextQty() {
        std::uniform_real_distribution<double> u(0.0, 1.0);
        double raw = params.quantity_min
                     * std::pow(1.0 - u(rng), -1.0 / params.quantity_alpha);
        raw = std::clamp(raw, params.quantity_min, params.quantity_max);
        return static_cast<Quantity>(std::round(raw));
    }


    TradeSide nextSide() {
        std::bernoulli_distribution d(params.buy_bias);
        return d(rng) ? TradeSide::Buy : TradeSide::Sell;
    }


    std::vector<BenchEvent> generateStream(std::size_t n_add) {
        std::vector<BenchEvent> events;
        events.reserve(static_cast<std::size_t>(
            static_cast<double>(n_add)
            * (1.0 + params.cancel_rate + params.modify_rate) * 1.1));

        std::vector<OrderId> live_ids;
        live_ids.reserve(n_add);

        std::bernoulli_distribution d_cancel(params.cancel_rate);
        std::bernoulli_distribution d_modify(params.modify_rate);

        for (std::size_t i = 0; i < n_add; ++i) {
            if (!live_ids.empty() && d_cancel(rng)) {
                OrderId victim = pickRandom(live_ids);
                events.emplace_back(CancelEvent{victim});
                live_ids.erase(std::ranges::find(live_ids, victim));
            }

            if (!live_ids.empty() && d_modify(rng)) {
                OrderId target = pickRandom(live_ids);
                Price mid = nextMid();
                Price hs = halfSpread();
                Price new_price = mid + (std_norm(rng) > 0.0 ? hs : -hs);
                new_price = std::max(static_cast<Price>(params.tick_size),
                                  static_cast<Price>(
                                      std::round(static_cast<double>(new_price)
                                                 / params.tick_size)
                                      * params.tick_size));
                events.emplace_back(ModifyEvent{
                    .targetId = target,
                    .new_qty = nextQty(),
                    .new_price = new_price
                });
            }
            Order order = generateOrder();
            events.emplace_back(AddEvent{order});

            if (order.getType() == OrderType::Limit) {
                live_ids.push_back(order.getId());

            }
        }

        return events;
    }

    Order generateOrder() {
        return generateOrder(generateOrderType());
    }

    Order generateOrder(OrderType orderType) {
        Price mid = nextMid();
        Price hs = halfSpread();
        TradeSide side = nextSide();
        Price price = (side == TradeSide::Buy) ? mid - hs : mid + hs;
        price = std::max(static_cast<Price>(params.tick_size),
                      static_cast<Price>(
                          std::round(static_cast<double>(price) / params.tick_size)
                          * params.tick_size));

        return {next_id++, orderType, nextQty(), price, side};
    }

    [[nodiscard]] OrderType generateOrderType() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double random_val = dist(rng_double);

        constexpr double EPSILON = 1e-9;
        for (const auto& [type, prob] : params.orderTypeProbability) {
            random_val -= prob;
            if (random_val <= EPSILON) {
                return type;
            }
        }

        return OrderType::Limit;
    }

    std::vector<Order> generateOrders(std::size_t n, OrderType fixed_type = OrderType::Limit) {
        std::vector<Order> out;
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            out.emplace_back(generateOrder(fixed_type));
        }
        return out;
    }

private:
    OrderId next_id {1};
    std::mt19937_64 rng;
    std::mt19937 rng_double;
    MarketParams params;
    double mid;

    std::normal_distribution<double> std_norm{0.0, 1.0};

    OrderId pickRandom(const std::vector<OrderId> &v) {
        std::uniform_int_distribution<std::size_t> d(0, v.size() - 1);
        return v[d(rng)];
    }
};
