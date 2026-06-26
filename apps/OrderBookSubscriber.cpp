#include "MulticastConfig.hpp"
#include "OrderBookUpdatesProtocol.hpp"
#include "asio_include.hpp"

#include <chrono>
#include <map>
#include <print>
#include <ranges>
#include <string_view>
#include <vector>

using namespace BookUpdateProtocol;

class Subscriber {
public:
    void apply(const SingleBookUpdateMessage &msg) {
        if (ticker == StockTicker{}) ticker = msg.ticker;
        auto update = [&](auto &level_map) {
            if (msg.price_level == PriceLevelAction::UPDATE)
                level_map[msg.price] = msg.quantity;
            else
                level_map.erase(msg.price);
        };
        msg.side == TradeSide::Buy ? update(bids) : update(asks);
    }

    void printMBP(int levels = 5) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_print_time <= print_interval) {
            return;
        }
        last_print_time = now;

        cleanConsole();
        std::println("=== Market By Price: {:.8} ===", std::string_view(ticker.data(), ticker.size()));

        std::println("ASK:");
        std::vector<std::pair<Price, Quantity> > top_asks;
        for (const auto &[price, qty]: asks | std::views::take(levels)) {
            top_asks.emplace_back(price, qty);
        }

        for (const auto &[price, qty]: top_asks | std::views::reverse) {
            std::println("  {:>12.4f}  |  {:>10}", static_cast<double>(price) / 10000.0, qty);
        }

        std::println("  {:->14}+{:->12}", "", "");
        std::println("BID:");
        for (const auto &[price, qty]: bids | std::views::take(levels)) {
            std::println("  {:>12.4f}  |  {:>10}", static_cast<double>(price) / 10000.0, qty);
        }
    }

private:
    static void cleanConsole() { std::print("\033[2J\033[H"); }

    std::chrono::time_point<std::chrono::steady_clock> last_print_time{};
    std::chrono::milliseconds print_interval{16};
    std::map<Price, Quantity, std::greater<> > bids;
    std::map<Price, Quantity> asks;
    StockTicker ticker{};
};


int main() {
    asio::io_context io;
    asio::ip::udp::endpoint listen_endpoint(asio::ip::address_v4::any(), MULTICAST_PORT);
    asio::ip::udp::socket socket(io, listen_endpoint.protocol());

    socket.set_option(asio::socket_base::reuse_address(true));
    socket.bind(listen_endpoint);
    socket.set_option(asio::ip::multicast::join_group(asio::ip::make_address(MULTICAST_GROUP)));

    std::println("Listening for MBP updates on {}:{}", MULTICAST_GROUP, MULTICAST_PORT);

    Subscriber order_book_subscriber{};

    std::array<char, MAX_DATAGRAM_PAYLOAD> buf{};
    asio::ip::udp::endpoint sender;
    std::uint32_t last_sequence_number{0};

    while (true) {
        std::size_t n = socket.receive_from(asio::buffer(buf), sender);
        if (sizeof(Header) > n) {
            continue;
        }

        const auto *header = std::start_lifetime_as<Header>(buf.data());
        if (header->sequence_number < last_sequence_number) {
            continue;
        }

        last_sequence_number = header->sequence_number;

        std::size_t expected_payload_size = sizeof(Header) + header->messages_count * sizeof(SingleBookUpdateMessage);
        if (expected_payload_size != n || header->type != MessageType::SingleBookUpdateMessage) {
            continue;
        }

        std::size_t offset{sizeof(Header)};

        for (std::uint16_t i = 0; i < header->messages_count; ++i) {
            const auto *msg = std::start_lifetime_as<SingleBookUpdateMessage>(buf.data() + offset);
            offset += sizeof(SingleBookUpdateMessage);
            order_book_subscriber.apply(*msg);
        }

        order_book_subscriber.printMBP();
    }
}
