#pragma once
#include "BaseTypes.hpp"


struct Trade {
    OrderId orderIdA;
    OrderId orderIdB;
    OrderId aggressorId;
    TradeSide aggressorSide;
    Price price;
    Quantity quantity;
    bool operator==(const Trade&) const = default;
};

