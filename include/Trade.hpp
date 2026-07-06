#pragma once
#include "BaseTypes.hpp"


struct Trade {
    OrderId order_id_a;
    OrderId order_id_b;
    OrderId aggressor_id;
    TradeSide aggressor_side;
    Price price;
    Quantity quantity;

    bool operator==(const Trade&) const = default;
};
