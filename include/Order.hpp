#pragma once
#include <cstdint>

enum class OrderType : std::uint8_t {
    Market,
    Limit,
    FillOrKill,
    ImmediateOrCancel,
    Iceberg,
};

enum class TradeSide : std::uint8_t {
    Buy,
    Sell
};

using Price = std::int64_t;
using Quantity = std::uint64_t;
using OrderId = std::uint64_t;

struct Order {
    OrderId id;
    OrderType type;
    Quantity initialQuantity;
    Price price;
    TradeSide side;
};

struct Trade {
    OrderId orderIdA;
    OrderId orderIdB;
    OrderId aggressorId;
    TradeSide aggressorSide;
    Price price;
    Quantity quantity;
    bool operator==(const Trade&) const;
};

