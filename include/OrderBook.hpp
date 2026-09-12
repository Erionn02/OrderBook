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
    OrderBook(std::size_t orders_reserve = static_cast<std::size_t>(std::pow(2,16)), std::size_t price_levels_reserve = 8192): bids(price_levels_reserve), asks(price_levels_reserve) {
        orders.reserve(orders_reserve);
        orders_cache.reserve(orders_reserve);
        for (std::size_t i = 0; i < orders_reserve; ++i) {
            orders_cache.push_back(&orders_source.emplace_back());
        }

        price_level_cache.reserve(price_levels_reserve*2);
        for (std::size_t i{0}; i < price_levels_reserve*2; ++i) {
            price_level_cache.push_back(&price_level_source.emplace_back());
        }
    }
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;

    [[gnu::flatten]] std::vector<Trade> addOrder(const Order& order);
    [[gnu::flatten]] void cancelOrder(OrderId orderId);
    [[gnu::flatten]] std::vector<Trade> modifyOrder(OrderId orderId, Quantity quantity, Price price);
    [[gnu::flatten]] std::vector<Trade> replaceOrder(OrderId oldOrderId, OrderId newOrderId, Quantity quantity, Price price);
    [[gnu::flatten]] void reduceExecutedOrder(OrderId orderId, Quantity reduced_quantity);

    std::size_t getOrdersCount() const;
    Order getOrder(OrderId orderId) const;
    
    int getBestBid() const {
        return bids.empty() ? -1 : bids.begin().key();
    }

    int getBestAsk() const {
        return asks.empty() ? -1 : asks.begin().key();
    }

    const auto& getOrders() const { return orders; }
    const auto& getBids() const { return bids; }
    const auto& getAsks() const { return asks; }
private:
    using OrderList = PriceLevel::OrderList;
    using OrderHashMap = boost::unordered_flat_map<OrderId, Order*>;

    [[gnu::always_inline]] inline std::vector<Trade> addOrderInternal(Order& intrusive_order, OrderHashMap::iterator slot);
    Order& getIntrusiveOrder(const Order& other);
    PriceLevel* allocatePriceLevel(Price price);
    PriceLevel* getPriceLevelOfLastOrder(Order& order);

    void cancelOrderInternal(OrderHashMap::iterator it);

    template<typename Comp, typename OrderMap, typename ToInsertMap>
    std::vector<Trade> addOrderImpl(Order& order, OrderHashMap::iterator slot, Comp&& price_comparator, OrderMap& order_map, ToInsertMap& to_insert_map) {
        switch (order.getType()) {
            case OrderType::Limit:
                return handleOrder<OrderType::Limit>(order, slot, std::forward<Comp>(price_comparator), order_map, to_insert_map);
            case OrderType::Market:
                return handleOrder<OrderType::Market>(order, slot, std::forward<Comp>(price_comparator), order_map, to_insert_map);
            case OrderType::ImmediateOrCancel:
                return handleOrder<OrderType::ImmediateOrCancel>(order, slot, std::forward<Comp>(price_comparator), order_map, to_insert_map);
            case OrderType::FillOrKill:
                if (canFillFillOrKillOrder(order, std::forward<Comp>(price_comparator), order_map)) {
                    return handleOrder<OrderType::FillOrKill>(order, slot, price_comparator, order_map, to_insert_map);
                }
                discardOrder(order, slot);
                return {};
            default:
                std::unreachable();
        }
    }

    void discardOrder(Order& order, OrderHashMap::iterator slot) {
        orders.erase(slot);
        orders_cache.push_back(&order);
    }

    template<OrderType order_type, typename Comp, typename OrderMap, typename ToInsertMap>
    std::vector<Trade> handleOrder(Order& order, OrderHashMap::iterator slot, [[maybe_unused]] Comp&& price_comparator, OrderMap& order_map,
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
                    orders_cache.push_back(&*existing_order_it);
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
                break;
            }
        }
        if constexpr (order_type == OrderType::Limit) {
            [[likely]] if (!order.isFilled()) {
                auto [price_level_it, _] = to_insert_map.get_or_insert(order.getPrice(), [&] { return allocatePriceLevel(order.getPrice()); });
                (*price_level_it)->orders.push_back(order);
                slot->second = &order;
                return trades;
            }
        }

        discardOrder(order, slot);
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


    packed_memory_array<Price, PriceLevel*, std::greater<>, NoOp, PMAMode::NoRebalance> bids{};
    packed_memory_array<Price, PriceLevel*, std::less<>, NoOp, PMAMode::NoRebalance> asks{};
    std::vector<Order*> orders_cache{};
    std::deque<Order, aligned_allocator<Order, 64>> orders_source{};
    std::vector<PriceLevel*> price_level_cache{};
    std::deque<PriceLevel, aligned_allocator<PriceLevel, 64>> price_level_source{};
    OrderHashMap orders{};
};
