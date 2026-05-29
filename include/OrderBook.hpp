#pragma once

#include <Order.hpp>
#include <vector>

class OrderBook {
public:
    std::vector<Trade> AddOrder(Order order);
    void CancelOrder(OrderId orderId);
};
