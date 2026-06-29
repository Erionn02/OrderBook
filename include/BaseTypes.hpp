#pragma once
#include <cstdint>

enum class OrderType : std::uint8_t {
    Market, // if there is not enough liquidity to fill the order, then rest of the order is cancelled
    Limit,
    FillOrKill,
    ImmediateOrCancel,
};

enum class TradeSide : std::uint8_t {
    Buy = 'B',
    Sell = 'S'
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
