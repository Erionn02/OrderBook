#pragma once
#include "Order.hpp"
#include "Trade.hpp"
#include "PriceLevel.hpp"

#include <vector>
#include <map>
#include <unordered_map>

class OrderBook {
public:
    std::vector<Trade> addOrder(Order order);
    void cancelOrder(OrderId orderId);
    void modifyOrder(OrderId orderId, Quantity quantity, Price price);

    std::size_t getOrdersCount() const;
    Order getOrder(OrderId orderId) const;
private:
    std::vector<Trade> addBuyOrder(Order order);
    std::vector<Trade> addSellOrder(Order order);

    std::map<Price, PriceLevel, std::greater<>> bids{};
    std::map<Price, PriceLevel, std::less<>> asks{};
    std::unordered_map<OrderId, decltype(PriceLevel::orders)::iterator> orders{};
};
