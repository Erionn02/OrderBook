#pragma once
#include "Order.hpp"
#include "Trade.hpp"
#include "PriceLevel.hpp"

#include <vector>
#include <map>
#include <unordered_map>
#include <utility>

class OrderBook {
public:
    OrderBook() = default;
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;

    std::vector<Trade> addOrder(Order order);
    void cancelOrder(OrderId orderId);
    std::vector<Trade> modifyOrder(OrderId orderId, Quantity quantity, Price price);

    std::size_t getOrdersCount() const;
    Order getOrder(OrderId orderId) const;
    const std::unordered_map<OrderId, decltype(PriceLevel::orders)::iterator>& getOrders() const { return orders; }
    const std::map<Price, PriceLevel, std::greater<>>& getBids() const { return bids; }
    const std::map<Price, PriceLevel, std::less<>>& getAsks() const { return asks; }
private:
    void cancelOrderInternal(std::unordered_map<OrderId, decltype(PriceLevel::orders)::iterator>::iterator it);

    template<typename Comp, typename OrderMap, typename ToInsertMap>
    std::vector<Trade> addOrderImpl(Order &order, Comp &&price_comparator, OrderMap& order_map, ToInsertMap& to_insert_map) {
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
    std::vector<Trade> handleOrder(Order &order, [[maybe_unused]] Comp && price_comparator, OrderMap& order_map, [[maybe_unused]] ToInsertMap& to_insert_map) {
        std::vector<Trade> trades;
        for (auto it = order_map.begin(); it != order_map.end();) {
            auto &[price, level] = *it;
            if constexpr (order_type == OrderType::Limit || order_type == OrderType::ImmediateOrCancel) {
                if (price_comparator(order.getPrice(), price)) {
                    break;
                }
            }
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
                it = order_map.erase(it);
            } else {
                ++it;
            }

            if (order.isFilled()) {
                return trades;
            }
        }
        if constexpr (order_type == OrderType::Limit) {
            auto &levelOrders = to_insert_map[order.getPrice()].orders;
            orders[order.getId()] = levelOrders.insert(levelOrders.end(), order);
        }
        return trades;
    }

    template<typename Comp, typename OrderMap>
    bool canFillFillOrKillOrder(const Order &order, Comp &&price_comparator, const OrderMap& order_map) const {
        Quantity canFill{0};
        for (auto &[price, level]: order_map) {
            if (price_comparator(order.getPrice(), price)) {
                break;
            }
            for (const auto& existing_order: level.orders) {
                canFill += existing_order.getQuantity();
                if (canFill >= order.getQuantity()) {
                    return true;
                }
            }
        }
        return false;
    }

    std::map<Price, PriceLevel, std::greater<> > bids{};
    std::map<Price, PriceLevel, std::less<> > asks{};
    std::unordered_map<OrderId, decltype(PriceLevel::orders)::iterator> orders{};
};
