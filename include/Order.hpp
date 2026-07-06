#pragma once
#include "BaseTypes.hpp"

#include <algorithm>

struct Order {
    Order() = default;

    Order(OrderId id, OrderType type, Quantity quantity, Price price, TradeSide side) : id(id),
                                                                                        initialQuantity(quantity), quantity(quantity), price(price),
                                                                                        type(type), side(side) {
    }

    Quantity fill(Order& other) {
        Quantity filled{std::min(other.quantity, quantity)};
        quantity -= filled;
        other.quantity -= filled;
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

    bool operator==(const Order& other) const = default;

private:
    OrderId id;
    Quantity initialQuantity;
    Quantity quantity;
    Price price;
    OrderType type;
    TradeSide side;
};
