#pragma once
#include "Order.hpp"
#include "Trade.hpp"
#include "PriceLevel.hpp"

#include <vector>
#include <utility>
#include <boost/unordered/unordered_flat_map.hpp>
#include <deque>

#include "packed_memory_array.hpp"

class OrderBook {
public:
    OrderBook(std::size_t orders_reserve = static_cast<std::size_t>(std::pow(2,15))) {
        orders.reserve(orders_reserve);
    }
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;

    std::vector<Trade> addOrder(Order order);
    void cancelOrder(OrderId orderId);
    std::vector<Trade> modifyOrder(OrderId orderId, Quantity quantity, Price price);
    std::vector<Trade> replaceOrder(OrderId oldOrderId, OrderId newOrderId, Quantity quantity, Price price);
    void reduceExecutedOrder(OrderId orderId, Quantity reduced_quantity);

    std::size_t getOrdersCount() const;
    Order getOrder(OrderId orderId) const;
    const auto& getOrders() const { return orders; }
    const auto& getBids() const { return bids; }
    const auto& getAsks() const { return asks; }
private:
    PriceLevel *allocatePriceLevel(Price price);

    using OrderHashMap = boost::unordered_flat_map<OrderId, std::pair<decltype(PriceLevel::orders)::iterator, PriceLevel*>>;

    void cancelOrderInternal(OrderHashMap::iterator it);

    template<typename Comp, typename OrderMap, typename ToInsertMap>
    std::vector<Trade> addOrderImpl(Order& order, Comp&& price_comparator, OrderMap& order_map, ToInsertMap& to_insert_map) {
        switch (order.getType()) {
            case OrderType::Limit:
                return handleOrder<OrderType::Limit>(order, std::forward<Comp>(price_comparator), order_map, to_insert_map);
            case OrderType::Market:
                return handleOrder<OrderType::Market>(order, std::forward<Comp>(price_comparator), order_map, to_insert_map);
            case OrderType::ImmediateOrCancel:
                return handleOrder<OrderType::ImmediateOrCancel>(order, std::forward<Comp>(price_comparator), order_map, to_insert_map);
            case OrderType::FillOrKill:
                if (canFillFillOrKillOrder(order, std::forward<Comp>(price_comparator), order_map)) {
                    return handleOrder<OrderType::FillOrKill>(order, price_comparator, order_map, to_insert_map);
                }
                return {};
            default:
                std::unreachable();
        }
    }

    template<OrderType order_type, typename Comp, typename OrderMap, typename ToInsertMap>
    std::vector<Trade> handleOrder(Order& order, [[maybe_unused]] Comp&& price_comparator, OrderMap& order_map,
                                   [[maybe_unused]] ToInsertMap& to_insert_map) {
        std::vector<Trade> trades;
        for (auto it = order_map.begin(); it != order_map.end();) {
            if constexpr (order_type == OrderType::Limit || order_type == OrderType::ImmediateOrCancel) {
                if (price_comparator(order.getPrice(), it.key())) {
                    break;
                }
            }
            auto& level = **it;
            auto existing_order_it = level.orders.begin();
            while (existing_order_it != level.orders.end() && !order.isFilled()) {
                Quantity filled = existing_order_it->fill(order);
                trades.emplace_back(order.getId(), existing_order_it->getId(), order.getId(), order.getSide(),
                                    existing_order_it->getPrice(), filled);
                if (existing_order_it->isFilled()) {
                    orders.erase(existing_order_it->getId());
                    existing_order_it = level.orders.erase(existing_order_it);
                } else {
                    ++existing_order_it;
                }
            }
            if (level.orders.empty()) {
                PriceLevel* lvl = *it;
                price_level_cache.push_back(lvl);
                it = order_map.erase(it);
            } else {
                ++it;
            }

            if (order.isFilled()) {
                return trades;
            }
        }
        if constexpr (order_type == OrderType::Limit) {
            auto [price_level_it, _] = to_insert_map.get_or_insert(order.getPrice(), [&] { return allocatePriceLevel(order.getPrice()); });
            auto& levelOrders = (*price_level_it)->orders;
            orders.emplace(order.getId(), std::pair{levelOrders.insert(levelOrders.end(), order), *price_level_it});
        }
        return trades;
    }

    template<typename Comp, typename OrderMap>
    bool canFillFillOrKillOrder(const Order& order, Comp&& price_comparator, const OrderMap& order_map) const {
        Quantity canFill{0};
        for (auto& level: order_map) {
            if (price_comparator(order.getPrice(), level->price)) {
                break;
            }
            for (const auto& existing_order: level->orders) {
                canFill += existing_order.getQuantity();
                if (canFill >= order.getQuantity()) {
                    return true;
                }
            }
        }
        return false;
    }

    std::vector<PriceLevel*> price_level_cache{};
    std::deque<PriceLevel> price_level_source{};

    struct UpdateIdxHook {
        void operator()(PriceLevel* level, std::size_t idx) {
            level->idx = idx;
        }
    };

    packed_memory_array<Price, PriceLevel*, std::greater<>, UpdateIdxHook> bids{};
    packed_memory_array<Price, PriceLevel*, std::less<>, UpdateIdxHook> asks{};

    OrderHashMap orders{};
};
