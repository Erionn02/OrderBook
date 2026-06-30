#include "OrderBook.hpp"


std::vector<Trade> OrderBook::addOrder(Order order) {
    if (orders.contains(order.getId())) {
        return {};
    }
    if (order.getSide() == TradeSide::Buy) {
        return addOrderImpl(order, std::less<Price>{}, asks, bids);
    }
    return addOrderImpl(order, std::greater<Price>{},bids, asks);
}

void OrderBook::cancelOrder(OrderId orderId) {
    auto it = orders.find(orderId);
    cancelOrderInternal(it);
}

void OrderBook::cancelOrderInternal(OrderHashMap::iterator it) {
    if (it != orders.end()) {
        auto orderIt = it->second.first;
        if (orderIt->getSide() == TradeSide::Buy) {
            auto level_it = it->second.second;
            PriceLevel& level = level_it->second;
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                bids.erase(level_it);
            }
        } else {
            auto level_it = it->second.second;
            PriceLevel& level = level_it->second;
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                asks.erase(level_it);
            }
        }
        orders.erase(it);
    }
}

std::vector<Trade> OrderBook::modifyOrder(OrderId orderId, Quantity quantity, Price price) {
    auto it = orders.find(orderId);
    if (it == orders.end()) {
        return {};
    }
    Order newOrder{orderId, (it->second.first)->getType(), quantity, price, (it->second.first)->getSide()};
    cancelOrderInternal(it);

    return addOrder(newOrder);
}

std::size_t OrderBook::getOrdersCount() const {
    return orders.size();
}

Order OrderBook::getOrder(OrderId orderId) const {
    return *orders.at(orderId).first;
}
