#pragma once

#include "Order.hpp"

#include <boost/pool/pool_alloc.hpp>
#include <boost/intrusive/list.hpp>

struct PriceLevel {
    Price price;
    boost::intrusive::list<Order> orders;
    std::size_t idx{std::string::npos};
};
