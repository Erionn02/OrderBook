#include "OrderBook.hpp"
#include "ItchParser.hpp"

#include <benchmark/benchmark.h>
#include <vector>
#include <ranges>
#include <print>

std::vector<ITCH::Message> parsed_itch_for_stock{};



static void BM_MixedStreamRealWorldData(benchmark::State &state) {
    std::size_t messages_processed{0};
    for (auto _: state) {
        messages_processed = 0;
        OrderBook book;
        for (const ITCH::Message &msg_variant: parsed_itch_for_stock) {
            std::visit([&]<typename T>(T && msg) {
                using RealType = std::remove_cvref_t<T>;
                if constexpr (std::is_same_v<RealType, ITCH::AddOrderMessage> || std::is_same_v<RealType, ITCH::AddOrderMPIDAttributionMessage>) {
                    ++messages_processed;
                    Order order{msg.order_reference_number, OrderType::Limit, msg.shares, static_cast<Price>(msg.price), std::bit_cast<TradeSide>(msg.side)};
                    book.addOrder(order);
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderExecutedMessage>) {
                    ++messages_processed;
                    Order order = book.getOrder(msg.order_reference_number);
                    if (order.getQuantity() > msg.executed_shares) {
                        book.modifyOrder(msg.order_reference_number, order.getQuantity() - msg.executed_shares, order.getPrice());
                    } else {
                        book.cancelOrder(msg.order_reference_number);
                    }
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderExecutedWithPriceMessage>) {
                    ++messages_processed;
                    Order order = book.getOrder(msg.order_reference_number);
                    if (order.getQuantity() > msg.shares) {
                        book.modifyOrder(msg.order_reference_number, order.getQuantity() - msg.shares, order.getPrice());
                    } else {
                        book.cancelOrder(msg.order_reference_number);
                    }
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderReplaceMessage>) {
                    Order old_order = book.getOrder(msg.original_order_reference_number);
                    book.cancelOrder(old_order.getId());
                    Order new_order{msg.new_order_reference_number, OrderType::Limit, msg.new_quantity, static_cast<Price>(msg.new_price), old_order.getSide()};
                    book.addOrder(new_order);
                    ++messages_processed;
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderDeleteMessage>) {
                    book.cancelOrder(msg.order_reference_number);
                    ++messages_processed;
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderCancelMessage>) {
                    Order order = book.getOrder(msg.order_reference_number);
                    if (order.getQuantity() > msg.cancelled_shares) {
                        book.modifyOrder(msg.order_reference_number, order.getQuantity() - msg.cancelled_shares, order.getPrice());
                    } else {
                        book.cancelOrder(msg.order_reference_number);
                    }
                    ++messages_processed;
                }
                // skip other types
            }, msg_variant);
        }
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(messages_processed) * state.iterations());
    state.counters["items_per_iter"] = benchmark::Counter(static_cast<double>(messages_processed), benchmark::Counter::kAvgIterations);
}

BENCHMARK(BM_MixedStreamRealWorldData)->Unit(benchmark::kSecond);


int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);

    if (argc < 2) {
        std::println("Usage: {} <path_to_filtered_itch_file>", argv[0]);
        return 1;
    }

    std::string filepath = argv[1];
    ITCH::ItchParser parser{filepath};
    parsed_itch_for_stock = parser.parseAll();

    if (parsed_itch_for_stock.empty()) {
        std::println("Parsed itch file to an empty vector");
        return 2;
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    return 0;
}