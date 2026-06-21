#include "ItchParser.hpp"

#include <print>


int main(int argc, char **argv) {
    if (argc != 4) {
        std::println("Usage: {} <input.NASDAQ_ITCH50> <TICKER> <output.itch>", argv[0]);
        std::println("e.g {} 12302019.NASDAQ_ITCH50 MSFT msft_20191230.itch", argv[0]);
        return 1;
    }
    const std::string itch_input_file{argv[1]};
    const std::string ticker_to_extract{argv[2]};
    const std::string output_itch_path{argv[3]};

    ITCH::ItchParser parser{itch_input_file};

    parser.parseAll([&](std::expected<ITCH::Message, std::string>&& message) {
        if (!message.has_value()) {
            std::println("Failed to parse message, details: {}", message.error());
        }
    });
}
