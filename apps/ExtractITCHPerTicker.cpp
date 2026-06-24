#include "ItchParser.hpp"

#include <print>
#include <optional>

ITCH::Stock_t createTickerOutOfCLI(const std::string& str) {
    ITCH::Stock_t ticker{};
    ticker.fill(' ');
    std::ranges::copy_n(str.begin(), static_cast<std::int64_t>(std::min(str.size(), ticker.size())), ticker.begin());
    return ticker;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        std::println("Usage: {} <input.NASDAQ_ITCH50> <TICKER> <output.itch>", argv[0]);
        std::println("e.g {} 12302019.NASDAQ_ITCH50 MSFT msft_20191230.itch", argv[0]);
        return 1;
    }
    const std::string itch_input_file{argv[1]};
    ITCH::Stock_t ticker{createTickerOutOfCLI(argv[2])};
    const std::string output_itch_path{argv[3]};

    std::ofstream fout{output_itch_path, std::ios::trunc | std::ios::binary};
    std::size_t written_messages_count{0};

    std::optional<ITCH::StockLocate_t> stock_locate{};

    ITCH::ItchParser parser{itch_input_file};
    parser.parseAll([&](std::expected<ITCH::Message, std::string> message) {
        if (!message.has_value()) {
            std::println("Failed to parse message, details: {}", message.error());
            return;
        }

        std::visit([&]<typename T>(T &&msg) {
            using RealMessageType = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<RealMessageType, ITCH::StockDirectoryMessage>) {
                if (msg.stock == ticker) {
                    stock_locate = msg.stock_locate;
                    std::println("Found stock_locate {} for {}", msg.stock_locate, ticker);
                }
            }

            if (stock_locate &&  msg.stock_locate == *stock_locate) {
                ++written_messages_count;
                std::uint16_t size_to_write = sizeof(RealMessageType::type) + sizeof(msg);
                if constexpr (ITCH::isLittleEndian()) {
                    size_to_write = __builtin_bswap16(size_to_write);
                    ITCH::swapMessageEndianness(msg);
                }
                fout.write(reinterpret_cast<const char *>(&size_to_write), sizeof(size_to_write));
                fout.write(reinterpret_cast<const char *>(&RealMessageType::type), sizeof(RealMessageType::type));
                fout.write(reinterpret_cast<const char *>(&msg), sizeof(msg));
            }

        }, *message);
    });

    std::println("Finished, wrote {} messages", written_messages_count);
    return written_messages_count == 0;
}
