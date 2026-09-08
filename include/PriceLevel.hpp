#pragma once

#include "Order.hpp"

#include <boost/intrusive/list.hpp>

struct alignas (32) PriceLevel {
    using OrderList = boost::intrusive::list<Order, boost::intrusive::constant_time_size<false>>;

    Price price;
    std::size_t idx{std::string::npos};
    OrderList orders;
};

static_assert(sizeof(PriceLevel) == 32);