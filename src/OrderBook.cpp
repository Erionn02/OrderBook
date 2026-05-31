#include "OrderBook.hpp"


std::vector<Trade> OrderBook::addOrder(Order order) {
    if (orders.contains(order.getId())) {
        return {};
    }
    if (order.getSide() == TradeSide::Buy) {
        return addBuyOrder(order);
    }
    return addSellOrder(order);
}

std::vector<Trade> OrderBook::addBuyOrder(Order order) {
    std::vector<Trade> trades;
    for (auto &[price, level] : asks) {
        if (order.getPrice() < price) {
            break;
        }
        auto sellOrderIt = level.orders.begin();
        while (sellOrderIt != level.orders.end())
        {
            Quantity filled = sellOrderIt->fill(order);
            trades.emplace_back(order.getId(), sellOrderIt->getId(), order.getId(), order.getSide(), sellOrderIt->getPrice(), filled);
            if (sellOrderIt->isFilled()) {
                orders.erase(sellOrderIt->getId());
                sellOrderIt = level.orders.erase(sellOrderIt);
            } else {
                ++sellOrderIt;
            }
            if (order.isFilled()) {
                return trades;
            }
        }
    }
    auto & levelOrders = bids[order.getPrice()].orders;
    orders[order.getId()] = levelOrders.insert(levelOrders.end(), order);
    return trades;
}

std::vector<Trade> OrderBook::addSellOrder(Order order) {
    std::vector<Trade> trades;
    for (auto &[price, level] : bids) {
        if (order.getPrice() > price) {
            break;
        }
        auto buyOrderIt = level.orders.begin();
        while (buyOrderIt != level.orders.end()) {
            Quantity filled = buyOrderIt->fill(order);
            trades.emplace_back(order.getId(), buyOrderIt->getId(), order.getId(), order.getSide(), buyOrderIt->getPrice(), filled);
            if (buyOrderIt->isFilled()) {
                orders.erase(buyOrderIt->getId());
                buyOrderIt = level.orders.erase(buyOrderIt);
            } else {
                ++buyOrderIt;
            }
            if (order.isFilled()) {
                return trades;
            }
        }
    }
    auto & levelOrders = asks[order.getPrice()].orders;
    orders[order.getId()] = levelOrders.insert(levelOrders.end(), order);
    return trades;
}

void OrderBook::cancelOrder(OrderId orderId) {
    auto it = orders.find(orderId);
    if (it != orders.end()) {
        auto orderIt = it->second;
        if (orderIt->getSide() == TradeSide::Buy) {
            PriceLevel& level = bids.at(orderIt->getPrice());
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                bids.erase(orderId);
            }
        } else {
            PriceLevel& level = asks.at(orderIt->getPrice());
            level.orders.erase(orderIt);
            if (level.orders.empty()) {
                asks.erase(orderId);
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
