#pragma once

#include "Order.hpp"

#include <boost/intrusive/list.hpp>

struct alignas (32) PriceLevel {
    using OrderList = boost::intrusive::list<Order, boost::intrusive::constant_time_size<false>>;

    Price price;
    OrderList orders;
};

static_assert(sizeof(PriceLevel) == 32);