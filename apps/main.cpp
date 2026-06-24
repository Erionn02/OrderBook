#include <iostream>

#include <chrono>
#include <print>

#include "ItchParser.hpp"

int main() {
    ITCH::ItchParser parser{"/home/kuba/CLionProjects/OrderBook/cache/12302019.NASDAQ_ITCH50"};
    std::size_t message_count{0};

    auto start = std::chrono::high_resolution_clock::now();

    parser.parseAll([&](auto&& message) {
        ++message_count;
        if (!message.has_value()) {
            std::println("Failed to parse message, details: {}", message.error());
        }
    });

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double speed = (double)message_count / (double)duration.count() / 1'000.0;
    std::println("Parsed {} messages in {}, speed: {} M/s", message_count, duration, speed);
}