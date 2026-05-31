#pragma once

#include <Order.hpp>
#include <list>

struct PriceLevel {
    Price price;
    std::list<Order> orders;
};
