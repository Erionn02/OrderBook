#include "ItchParser.hpp"

#include <benchmark/benchmark.h>
#include <filesystem>
#include <print>

std::string filepath;


static void BM_ItchParser(benchmark::State &state) {
    std::size_t messages_processed{0};
    for (auto _: state) {
        state.PauseTiming();
        ITCH::ItchParser parser{filepath};
        state.ResumeTiming();

        parser.parseAll([&](auto&&) {
            ++messages_processed;
        });
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(messages_processed));
}

BENCHMARK(BM_ItchParser)->Unit(benchmark::kSecond);


int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);

    if (argc < 2) {
        std::println("Usage: {} <path_to_itch_file>", argv[0]);
        return 1;
    }

    filepath = argv[1];
    if (!std::filesystem::exists(filepath)) {
        std::println("File does not exist: {}", filepath);
        return 2;
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    return 0;
}