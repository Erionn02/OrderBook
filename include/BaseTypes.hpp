#pragma once
#include <cstdint>

// will be implemented later
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
