#pragma once

#include "Order.hpp"

#include <list>
#include <boost/pool/pool_alloc.hpp>

template<typename T>
struct AlignedNewDelete
{
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    static char * malloc (const size_type bytes) noexcept
    {
        return static_cast<char*>(operator new(bytes, std::align_val_t(alignof(T)), std::nothrow));
    }
    static void free(char * const block) noexcept
    {
        operator delete(block, alignof(T));
    }
};

using OrderListAllocator = boost::fast_pool_allocator<Order, AlignedNewDelete<Order>, boost::details::pool::null_mutex>;

struct PriceLevel {
    Price price;
    std::list<Order, OrderListAllocator> orders;
    std::size_t idx{std::string::npos};
};
