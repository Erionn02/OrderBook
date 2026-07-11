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
    [[likely]] if (it != orders.end()) {
        cancelOrderInternal(it);
    }
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
    auto [orderIt, level] = it->second;
    if (orderIt->getSide() == TradeSide::Buy) {
        level->orders.erase(orderIt);
        [[unlikely]] if (level->orders.empty()) {
            price_level_cache.push_back(level);
            using itType = std::remove_cvref_t<decltype(bids)>::iterator;
            bids.erase(itType(&bids, level->idx));
        }
    } else {
        level->orders.erase(orderIt);
        [[unlikely]] if (level->orders.empty()) {
            price_level_cache.push_back(level);
            using itType = std::remove_cvref_t<decltype(asks)>::iterator;
            asks.erase(itType(&asks, level->idx));
        }
    }
    orders.erase(it);
}

std::vector<Trade> OrderBook::modifyOrder(OrderId orderId, Quantity quantity, Price price) {
    auto it = orders.find(orderId);
    [[unlikely]] if (it == orders.end()) {
        return {};
    }
    auto [orderIt, level] = it->second;
    Order& oldOrder = *orderIt;
    Order newOrder{orderId, oldOrder.getType(), quantity, price, oldOrder.getSide()};
    if (price == oldOrder.getPrice()) {
        if (quantity > oldOrder.getQuantity()) {
            level->orders.erase(it->second.first);
            it->second.first = level->orders.insert(level->orders.end(), newOrder);
        } else {
            oldOrder = newOrder;
        }
        return {};
    }
    cancelOrderInternal(it);
    return addOrder(newOrder);
}

std::vector<Trade> OrderBook::replaceOrder(OrderId oldOrderId, OrderId newOrderId, Quantity quantity, Price price) {
    auto it = orders.find(oldOrderId);
    [[unlikely]] if (it == orders.end()) {
        return {};
    }
    Order newOrder{newOrderId, it->second.first->getType(), quantity, price, it->second.first->getSide()};
    cancelOrderInternal(it);
    return addOrder(newOrder);
}

void OrderBook::reduceExecutedOrder(OrderId orderId, Quantity reduced_quantity) {
    auto it = orders.find(orderId);
    [[unlikely]] if (it == orders.end()) {
        return;
    }
    Order& order = *it->second.first;
    order.fill(reduced_quantity);
    if (order.isFilled()) {
        cancelOrderInternal(it);
    }
}

std::size_t OrderBook::getOrdersCount() const {
    return orders.size();
}

Order OrderBook::getOrder(OrderId orderId) const {
    return *orders.at(orderId).first;
}
