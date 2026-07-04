#include "OrderBook.hpp"


std::vector<Trade> OrderBook::addOrder(Order order) {
    if (orders.contains(order.getId())) {
        return {};
    }
    if (order.getSide() == TradeSide::Buy) {
        return addOrderImpl(order, std::less<Price>{}, asks, bids);
    }
    return addOrderImpl(order, std::greater<Price>{}, bids, asks);
}

void OrderBook::cancelOrder(OrderId orderId) {
    auto it = orders.find(orderId);
    cancelOrderInternal(it);
}

void OrderBook::cancelOrderInternal(OrderHashMap::iterator it) {
    if (it != orders.end()) {
        auto orderIt = it->second;
        if (orderIt->getSide() == TradeSide::Buy) {
            auto level_it = bids.find(orderIt->getPrice());
            PriceLevel &level = *level_it;
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                bids.erase(level_it);
            }
        } else {
            auto level_it = asks.find(orderIt->getPrice());
            PriceLevel &level = *level_it;
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
    Order newOrder{orderId, it->second->getType(), quantity, price, it->second->getSide()};
    cancelOrderInternal(it);

    return addOrder(newOrder);
}

std::size_t OrderBook::getOrdersCount() const {
    return orders.size();
}

Order OrderBook::getOrder(OrderId orderId) const {
    return *orders.at(orderId);
}
