#pragma once
#include "BaseTypes.hpp"

#include <algorithm>
#include <utility>
#include <boost/intrusive/list.hpp>

namespace bi = boost::intrusive;

struct Order : bi::list_base_hook<bi::link_mode<bi::normal_link>> {
    Order() = default;

    Order(OrderId id, OrderType type, Quantity quantity, Price price, TradeSide side) : list_base_hook(), id(id),
                                                                                        initialQuantity(quantity), quantity(quantity), price(price),
                                                                                        type(type), side(side) {
    }

    Quantity fill(Order& other) {
        Quantity filled{std::min(other.quantity, quantity)};
        quantity -= filled;
        other.quantity -= filled;
        return filled;
    }

    Quantity fill(Quantity to_fill) {
        Quantity filled{std::min(to_fill, quantity)};
        quantity -= filled;
        return filled;
    }

    [[nodiscard]] Quantity getFilled() const {
        return initialQuantity - quantity;
    }

    [[nodiscard]] bool isFilled() const {
        return quantity == 0;
    }

    [[nodiscard]] OrderId getId() const {
        return id;
    }

    [[nodiscard]] OrderType getType() const {
        return type;
    }

    [[nodiscard]] Quantity getInitialQuantity() const {
        return initialQuantity;
    }

    [[nodiscard]] Quantity getQuantity() const {
        return quantity;
    }

    [[nodiscard]] Price getPrice() const {
        return price;
    }

    [[nodiscard]] TradeSide getSide() const {
        return side;
    }

    bool operator==(const Order& other) const {
        return std::tie(id, initialQuantity, quantity, price, type, side) == std::tie(other.id, other.initialQuantity, other.quantity, other.price,
                                                                                      other.type, other.side);
    }

private:
    OrderId id;
    Quantity initialQuantity;
    Quantity quantity;
    Price price;
    OrderType type;
    TradeSide side;
};
