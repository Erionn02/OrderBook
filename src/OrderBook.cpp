#include "OrderBook.hpp"
#include <boost/intrusive/parent_from_member.hpp>

std::vector<Trade> OrderBook::addOrder(const Order& order) {
    auto [slot, inserted] = orders.try_emplace(order.getId(), nullptr);
    [[unlikely]] if (!inserted) {
        return {};
    }
    return addOrderInternal(getIntrusiveOrder(order), slot);
}

std::vector<Trade> OrderBook::addOrderInternal(Order& intrusive_order, OrderHashMap::iterator slot) {
    if (intrusive_order.getSide() == TradeSide::Buy) {
        return addOrderImpl(intrusive_order, slot, std::less<Price>{}, asks, bids);
    }
    return addOrderImpl(intrusive_order, slot, std::greater<Price>{}, bids, asks);
}

void OrderBook::cancelOrder(OrderId orderId) {
    auto it = orders.find(orderId);
    [[likely]] if (it != orders.end()) {
        cancelOrderInternal(it);
    }
}

Order& OrderBook::getIntrusiveOrder(const Order& other) {
    if (!orders_cache.empty()) {
        auto last = orders_cache.back();
        orders_cache.pop_back();
        return *last = other;
    }
    return orders_source.emplace_back(other);
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

PriceLevel* OrderBook::getPriceLevelOfLastOrder(Order& order) {
    OrderList& list = OrderList::container_from_end_iterator(std::next(OrderList::s_iterator_to(order)));
    return boost::intrusive::get_parent_from_member(&list, &PriceLevel::orders);
}

void OrderBook::cancelOrderInternal(OrderHashMap::iterator it) {
    using node_traits = OrderList::node_traits;
    Order* order = it->second;
    auto side = order->getSide();
    orders.erase(it);
    orders_cache.push_back(order);

    auto node = OrderList::value_traits::to_node_ptr(*order);
    auto next = node_traits::get_next(node);
    auto prev = node_traits::get_previous(node);
    node_traits::set_next(prev, next);
    node_traits::set_previous(next, prev);

    [[unlikely]]
    if (prev == next) {
        PriceLevel* level = getPriceLevelOfLastOrder(*order);
        price_level_cache.push_back(level);
        if (side == TradeSide::Buy) {
            bids.erase(level->price);
        } else {
            asks.erase(level->price);
        }
    }
}

std::vector<Trade> OrderBook::modifyOrder(OrderId orderId, Quantity quantity, Price price) {
    auto it = orders.find(orderId);
    [[unlikely]] if (it == orders.end()) {
        return {};
    }
    Order& oldOrder = *it->second;
    Order newOrder{orderId, oldOrder.getType(), quantity, price, oldOrder.getSide()};
    if (price == oldOrder.getPrice()) {
        if (quantity > oldOrder.getQuantity()) {
            OrderList& level_orders = oldOrder.getSide() == TradeSide::Buy ? (*bids.find(oldOrder.getPrice()))->orders : (*asks.find(oldOrder.getPrice()))->orders;
            level_orders.erase(OrderList::s_iterator_to(oldOrder));
            oldOrder = newOrder;
            level_orders.push_back(oldOrder);
        } else {
            oldOrder = newOrder;
        }
        return {};
    }
    cancelOrderInternal(it);
    auto [slot, _] = orders.try_emplace(orderId, nullptr);
    return addOrderInternal(getIntrusiveOrder(newOrder), slot);
}

std::vector<Trade> OrderBook::replaceOrder(OrderId oldOrderId, OrderId newOrderId, Quantity quantity, Price price) {
    auto it = orders.find(oldOrderId);
    [[unlikely]] if (it == orders.end()) {
        return {};
    }
    Order newOrder{newOrderId, it->second->getType(), quantity, price, it->second->getSide()};
    cancelOrderInternal(it);
    auto [slot, inserted] = orders.try_emplace(newOrderId, nullptr);
    [[unlikely]] if (!inserted) {
        return {};
    }
    return addOrderInternal(getIntrusiveOrder(newOrder), slot);
}

void OrderBook::reduceExecutedOrder(OrderId orderId, Quantity reduced_quantity) {
    auto it = orders.find(orderId);
    [[unlikely]] if (it == orders.end()) {
        return;
    }
    Order& order = *it->second;
    order.fill(reduced_quantity);
    if (order.isFilled()) {
        cancelOrderInternal(it);
    }
}

std::size_t OrderBook::getOrdersCount() const {
    return orders.size();
}

Order OrderBook::getOrder(OrderId orderId) const {
    return *orders.at(orderId);
}
