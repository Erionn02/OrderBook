#pragma once

#include "Order.hpp"

#include <list>
#include <boost/pool/pool_alloc.hpp>

using OrderListAllocator = boost::fast_pool_allocator<Order, boost::default_user_allocator_new_delete, boost::details::pool::null_mutex>;

struct PriceLevel {
    Price price;
    std::list<Order, OrderListAllocator> orders;
};
