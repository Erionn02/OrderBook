#include "OrderBook.hpp"
#include "ItchParser.hpp"
#include "LatencyRecorder.hpp"

#include <benchmark/benchmark.h>
#include <vector>
#include <print>

std::vector<ITCH::Message> parsed_itch_for_stock{};


template<bool record_latency>
static void BM_MixedStreamRealWorldData(benchmark::State &state) {
    std::size_t messages_processed{0};
    LatencyRecorder latency_recorder;
    for (auto _: state) {
        OrderBook book{};
        for (const ITCH::Message &msg_variant: parsed_itch_for_stock) {
            std::visit([&]<typename T>(T && msg) {
                using RealType = std::remove_cvref_t<T>;
                if constexpr (std::is_same_v<RealType, ITCH::AddOrderMessage> || std::is_same_v<RealType, ITCH::AddOrderMPIDAttributionMessage>) {
                    ++messages_processed;
                    RecordOperationLambda([&]{
                        Order order{msg.order_reference_number, OrderType::Limit, msg.shares, static_cast<Price>(msg.price), std::bit_cast<TradeSide>(msg.side)};
                        book.addOrder(order);
                    })
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderExecutedMessage>) {
                    ++messages_processed;
                    RecordOperationLambda ([&] {
                        Order order = book.getOrder(msg.order_reference_number);
                        if (order.getQuantity() > msg.executed_shares) {
                            book.modifyOrder(msg.order_reference_number, order.getQuantity() - msg.executed_shares, order.getPrice());
                        } else {
                            book.cancelOrder(msg.order_reference_number);
                        }
                    })
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderExecutedWithPriceMessage>) {
                    ++messages_processed;
                    RecordOperationLambda([&] {
                        Order order = book.getOrder(msg.order_reference_number);
                        if (order.getQuantity() > msg.shares) {
                            book.modifyOrder(msg.order_reference_number, order.getQuantity() - msg.shares, order.getPrice());
                        } else {
                            book.cancelOrder(msg.order_reference_number);
                        }
                    })
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderReplaceMessage>) {
                    ++messages_processed;
                    RecordOperationLambda([&] {
                        Order old_order = book.getOrder(msg.original_order_reference_number);
                        book.cancelOrder(old_order.getId());
                        Order new_order{msg.new_order_reference_number, OrderType::Limit, msg.new_quantity, static_cast<Price>(msg.new_price), old_order.getSide()};
                        book.addOrder(new_order);
                    })
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderDeleteMessage>) {
                    ++messages_processed;
                    RecordOperationLambda([&] {
                        book.cancelOrder(msg.order_reference_number);
                    })
                } else if constexpr (std::is_same_v<RealType, ITCH::OrderCancelMessage>) {
                    ++messages_processed;
                    RecordOperationLambda([&] {
                        Order order = book.getOrder(msg.order_reference_number);
                        if (order.getQuantity() > msg.cancelled_shares) {
                            book.modifyOrder(msg.order_reference_number, order.getQuantity() - msg.cancelled_shares, order.getPrice());
                        } else {
                            book.cancelOrder(msg.order_reference_number);
                        }
                    })
                }
                // skip other types
            }, msg_variant);
        }
    }

    if constexpr (record_latency) {
        reportLatency(state, latency_recorder);
    } else {
        state.SetItemsProcessed(static_cast<std::int64_t>(messages_processed));
    }
}

BENCHMARK(BM_MixedStreamRealWorldData<false>)->Unit(benchmark::kSecond)->Name("BM_MixedStreamRealWorldData");
BENCHMARK(BM_MixedStreamRealWorldData<true>)->Unit(benchmark::kSecond)->Name("BM_MixedStreamRealWorldDataLatency");

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