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

void OrderBook::cancelOrderInternal(boost::unordered_flat_map<OrderId, decltype(PriceLevel::orders)::iterator>::iterator it) {
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

std::vector<Trade> OrderBook::modifyOrder(OrderId orderId, Quantity quantity, Price price) {
    auto it = orders.find(orderId);
    if (it == orders.end()) {
        return {};
    }
    Order newOrder{orderId, (it->second)->getType(), quantity, price, (it->second)->getSide()};
    cancelOrderInternal(it);

    return addOrder(newOrder);
}

std::size_t OrderBook::getOrdersCount() const {
    return orders.size();
}

Order OrderBook::getOrder(OrderId orderId) const {
    return *orders.at(orderId);
}
