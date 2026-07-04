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

PriceLevel* OrderBook::allocatePriceLevel(Price price) {
    if (!price_level_cache.empty()) {
        auto last = price_level_cache.back();
        price_level_cache.pop_back();
        last->price = price;
        return last;
    }
    auto level = &price_level_source.emplace_back();
    level->price = price;
    return level;
}

void OrderBook::cancelOrderInternal(OrderHashMap::iterator it) {
    if (it != orders.end()) {
        auto orderIt = it->second;
        if (orderIt->getSide() == TradeSide::Buy) {
            auto level_it = bids.find(orderIt->getPrice());
            PriceLevel &level = **level_it;
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                price_level_cache.push_back(*level_it);
                bids.erase(level_it);
            }
        } else {
            auto level_it = asks.find(orderIt->getPrice());
            PriceLevel &level = **level_it;
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                price_level_cache.push_back(*level_it);
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
