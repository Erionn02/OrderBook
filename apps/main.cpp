#include "ItchParser.hpp"

int main() {
    ITCH::ItchParser parser{"/home/kuba/Projects/OrderBook/downloads/12302019.NASDAQ_ITCH50"};
    std::size_t message_count{0};
    while (std::expected<ITCH::Message, std::string_view> msg = parser.parseNext()) {
        if (++message_count % 1'000'000 == 0) {
            ITCH::printMessage(*msg);
        }
    }
    std::print("Messages count: {}\n", message_count);
}