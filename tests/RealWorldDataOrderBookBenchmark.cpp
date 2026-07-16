#include "OrderBook.hpp"
#include "ItchParser.hpp"
#include "LatencyRecorder.hpp"

#include <benchmark/benchmark.h>
#include <vector>
#include <print>
#include <fcntl.h>
#include <thread>


void runPerf(bool record) {
    pid_t parent_pid = getpid();
    int pipefd[2];
    pipe2(pipefd, O_CLOEXEC);
    pid_t pid = fork();

    if (pid == 0) {
        close(pipefd[0]);
        struct sched_param param;
        param.sched_priority = 0;
        sched_setscheduler(0, SCHED_OTHER, &param);
        unsetenv("LD_PRELOAD");

        std::string pid_str{std::to_string(parent_pid)};
        int ret{0};
        if (record) {
#define PERF_RECORD_ARGS "perf", "record", "--call-graph", "dwarf", "-p", pid_str.c_str(), "-o", "perf.data", nullptr
            if (geteuid() == 0) {
                const char* const argv[] = {"taskset", "-c", "1", PERF_RECORD_ARGS};
                ret = execvp("taskset", const_cast<char* const*>(argv));
            } else {
                const char* const argv[] = {PERF_RECORD_ARGS};
                ret = execvp("perf", const_cast<char* const*>(argv));
            }
        } else {
#define PERF_STAT_ARGS "perf", "stat", "-d", "-d", "-d", "-p", pid_str.c_str(), "-o", "perf_report.txt", nullptr
            if (geteuid() == 0) {
                const char* const argv[] = {"taskset", "-c", "1", PERF_STAT_ARGS};
                ret = execvp("taskset", const_cast<char* const*>(argv));
            } else {
                const char* const argv[] = {PERF_STAT_ARGS};
                ret = execvp("perf", const_cast<char* const*>(argv));
            }
        }
        std::print("Error: {}", ret);
    }
    close(pipefd[1]);
    char c;
    read(pipefd[0], &c, 1);
    close(pipefd[0]);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

std::vector<ITCH::Message> parsed_itch_for_stock{};


template<bool record_latency>
static void BM_MixedStreamRealWorldData(benchmark::State &state) {
    std::size_t messages_processed{0};
    LatencyRecorder latency_recorder;
    for (auto _: state) {
        state.PauseTiming();
        OrderBook book{};
        state.ResumeTiming();
        for (const ITCH::Message &msg_variant: parsed_itch_for_stock) {
            if (auto* msg = std::get_if<ITCH::AddOrderMessage>(&msg_variant)) {
                ++messages_processed;
                RecordOperation(book.addOrder(Order{msg->order_reference_number, OrderType::Limit, msg->shares, static_cast<Price>(msg->price), std::bit_cast<TradeSide>(msg->side)}))
            } else if (auto* msg = std::get_if<ITCH::AddOrderMPIDAttributionMessage>(&msg_variant)) {
                ++messages_processed;
                RecordOperation(book.addOrder(Order{msg->order_reference_number, OrderType::Limit, msg->shares, static_cast<Price>(msg->price), std::bit_cast<TradeSide>(msg->side)}))
            } else if (auto* msg = std::get_if<ITCH::OrderExecutedMessage>(&msg_variant)) {
                ++messages_processed;
                RecordOperation(book.reduceExecutedOrder(msg->order_reference_number, msg->executed_shares))
            } else if (auto* msg = std::get_if<ITCH::OrderExecutedWithPriceMessage>(&msg_variant)) {
                ++messages_processed;
                RecordOperation(book.reduceExecutedOrder(msg->order_reference_number, msg->executed_shares))
            } else if (auto* msg = std::get_if<ITCH::OrderDeleteMessage>(&msg_variant)) {
                ++messages_processed;
                RecordOperation(book.cancelOrder(msg->order_reference_number))
            } else if (auto* msg = std::get_if<ITCH::OrderCancelMessage>(&msg_variant)) {
                ++messages_processed;
                RecordOperation(book.reduceExecutedOrder(msg->order_reference_number, msg->cancelled_shares))
            } else if (auto* msg = std::get_if<ITCH::OrderReplaceMessage>(&msg_variant)) {
                ++messages_processed;
                RecordOperation(book.replaceOrder(msg->original_order_reference_number, msg->new_order_reference_number, msg->new_quantity, static_cast<Price>(msg->new_price)))
            }
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

    for (int i{2}; i < argc; i++) {
        if (std::strcmp(argv[i], "--perf-record") == 0) {
            std::println("Running with perf");
            runPerf(true);
            break;
        }
        if (std::strcmp(argv[i], "--perf-stat") == 0) {
            std::println("Running with perf");
            runPerf(false);
            break;
        }
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    return 0;
}