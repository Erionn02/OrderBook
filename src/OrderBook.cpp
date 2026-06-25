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
    if (it != orders.end()) {
        auto orderIt = it->second;
        if (orderIt->getSide() == TradeSide::Buy) {
            PriceLevel& level = bids.at(orderIt->getPrice());
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                bids.erase(orderIt->getPrice());
            }
        } else {
            PriceLevel& level = asks.at(orderIt->getPrice());
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                asks.erase(orderIt->getPrice());
            }
        }
        orders.erase(it);
    }
}

void OrderBook::modifyOrder(OrderId orderId, Quantity quantity, Price price) {
    auto& order = *orders.at(orderId);
    Order newOrder{orderId, order.getType(), quantity, price, order.getSide()};
    cancelOrder(order.getId());
    addOrder(newOrder);
}

std::size_t OrderBook::getOrdersCount() const {
    return orders.size();
}

Order OrderBook::getOrder(OrderId orderId) const {
    return *orders.at(orderId);
}
