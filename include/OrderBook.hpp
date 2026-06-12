#pragma once
#include "Order.hpp"
#include "Trade.hpp"
#include "PriceLevel.hpp"

#include <vector>
#include <map>
#include <unordered_map>

class OrderBook {
public:
    OrderBook() = default;
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;

    std::vector<Trade> addOrder(Order order);
    void cancelOrder(OrderId orderId);
    void modifyOrder(OrderId orderId, Quantity quantity, Price price);
    std::size_t getOrdersCount() const;
    Order getOrder(OrderId orderId) const;
    const std::unordered_map<OrderId, decltype(PriceLevel::orders)::iterator>& getOrders() const { return orders; }

private:
    template<typename Comp, typename OrderMap, typename ToInsertMap>
    std::vector<Trade> addOrderImpl(Order &order, Comp &&price_comparator, OrderMap& order_map, ToInsertMap& to_insert_map) {
        switch (order.getType()) {
            case OrderType::Market:
                return handleMarketOrder(order, std::forward<Comp>(price_comparator), order_map, to_insert_map);
            case OrderType::FillOrKill:
                return handleFillOrKill(order, std::forward<Comp>(price_comparator), order_map);
            default:
                return {};
        }
    }

    template<typename Comp, typename OrderMap, typename ToInsertMap>
    std::vector<Trade> handleMarketOrder(Order &order, Comp &&price_comparator, OrderMap& order_map, ToInsertMap& to_insert_map) {
        std::vector<Trade> trades;
        for (auto &[price, level]: order_map) {
            if (price_comparator(order.getPrice(), price)) {
                break;
            }
            auto existing_order_it = level.orders.begin();
            while (existing_order_it != level.orders.end()) {
                Quantity filled = existing_order_it->fill(order);
                trades.emplace_back(order.getId(), existing_order_it->getId(), order.getId(), order.getSide(),
                                    existing_order_it->getPrice(), filled);
                if (existing_order_it->isFilled()) {
                    orders.erase(existing_order_it->getId());
                    existing_order_it = level.orders.erase(existing_order_it);
                } else {
                    ++existing_order_it;
                }
                if (order.isFilled()) {
                    return trades;
                }
            }
        }

        auto &levelOrders = to_insert_map[order.getPrice()].orders;
        orders[order.getId()] = levelOrders.insert(levelOrders.end(), order);
        return trades;
    }

    template<typename Comp, typename OrderMap>
    std::vector<Trade> handleFillOrKill(Order &order, Comp &&price_comparator, OrderMap& order_map) {
        Quantity needToFill = order.getQuantity();
        Quantity canFill{0};
        std::size_t ordersToTradeCount{0};
        for (auto &[price, level]: order_map) {
            if (price_comparator(order.getPrice(), price)) {
                break;
            }
            auto existing_order_it = level.orders.begin();
            while (existing_order_it != level.orders.end()) {
                canFill += existing_order_it->getQuantity();
                ++ordersToTradeCount;
                if (canFill >= needToFill) {
                    return fillFillOrKillOrder(order, order_map, ordersToTradeCount);
                }
                ++existing_order_it;
            }
        }
        return {};
    }

    template<typename OrderMap> // caller promises that it's possible to fill that order
    std::vector<Trade> fillFillOrKillOrder(Order &order, OrderMap& order_map, std::size_t ordersToTradeCount) {
        std::vector<Trade> trades;
        trades.reserve(ordersToTradeCount);
        for (auto &[price, level]: order_map) {
            auto existing_order_it = level.orders.begin();
            while (existing_order_it != level.orders.end() && ordersToTradeCount--) {
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
        }
        return trades;
    }



    std::map<Price, PriceLevel, std::greater<> > bids{};
    std::map<Price, PriceLevel, std::less<> > asks{};
    std::unordered_map<OrderId, decltype(PriceLevel::orders)::iterator> orders{};
};
