#include "MulticastConfig.hpp"
#include "OrderBookUpdatesProtocol.hpp"
#include "OrderBook.hpp"
#include "ItchParser.hpp"
#include "asio_include.hpp"

#include <chrono>
#include <concepts>
#include <cstring>
#include <numeric>
#include <print>
#include <vector>

using namespace BookUpdateProtocol;


std::size_t getTimestampNs() {
    return static_cast<std::size_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

Quantity levelTotal(const PriceLevel &level) {
    return std::accumulate(level.orders.begin(), level.orders.end(), Quantity{0},
                           [](Quantity sum, const Order &o) { return sum + o.getQuantity(); });
}

Quantity getDelta(const ITCH::OrderExecutedMessage &msg) { return msg.executed_shares; }
Quantity getDelta(const ITCH::OrderExecutedWithPriceMessage &msg) { return msg.shares; }
Quantity getDelta(const ITCH::OrderCancelMessage &msg) { return msg.cancelled_shares; }

class Publisher {
public:
    Publisher(asio::ip::udp::socket &s, asio::ip::udp::endpoint &ep) : socket(s), endpoint(ep) {
        pending.reserve(MAX_MSGS_PER_PACKET);
    }

    template<typename T>
        requires std::same_as<T, ITCH::AddOrderMessage> ||
                 std::same_as<T, ITCH::AddOrderMPIDAttributionMessage>
    void handle(const T &msg) {
        if (ticker == StockTicker{}) {
            ticker = msg.stock;
        }
        TradeSide side = (msg.side == ITCH::Side::Buy) ? TradeSide::Buy : TradeSide::Sell;
        auto price = static_cast<Price>(msg.price);
        book.addOrder({msg.order_reference_number, OrderType::Limit, msg.shares, price, side});
        sendLevelUpdate(price, side);
    }

    template<typename T>
        requires std::same_as<T, ITCH::OrderExecutedMessage> ||
                 std::same_as<T, ITCH::OrderExecutedWithPriceMessage> ||
                 std::same_as<T, ITCH::OrderCancelMessage>
    void handle(const T &msg) {
        if (!book.getOrders().contains(msg.order_reference_number)) {
            return;
        }
        Order o = book.getOrder(msg.order_reference_number);
        Quantity delta = getDelta(msg);
        if (o.getQuantity() > delta) {
            book.modifyOrder(o.getId(), o.getQuantity() - delta, o.getPrice());
        } else {
            book.cancelOrder(o.getId());
        }
        sendLevelUpdate(o.getPrice(), o.getSide());
    }

    void handle(const ITCH::OrderDeleteMessage &msg) {
        if (!book.getOrders().contains(msg.order_reference_number)) {
            return;
        }
        Order o = book.getOrder(msg.order_reference_number);
        book.cancelOrder(msg.order_reference_number);
        sendLevelUpdate(o.getPrice(), o.getSide());
    }

    void handle(const ITCH::OrderReplaceMessage &msg) {
        if (!book.getOrders().contains(msg.original_order_reference_number)) {
            return;
        }
        Order old = book.getOrder(msg.original_order_reference_number);
        Price old_price = old.getPrice();
        TradeSide side = old.getSide();
        book.cancelOrder(msg.original_order_reference_number);
        sendLevelUpdate(old_price, side);
        auto new_price = static_cast<Price>(msg.new_price);
        book.addOrder({
            msg.new_order_reference_number, OrderType::Limit,
            msg.new_quantity, new_price, side
        });
        sendLevelUpdate(new_price, side);
    }

    void handle(const auto &) {
    } // ignore all other ITCH message types

    void flush() {
        if (pending.empty()) {
            return;
        }
        std::array<char, MAX_DATAGRAM_PAYLOAD> buf{};
        Header header{
            .send_time = getTimestampNs(),
            .sequence_number = seq_num++,
            .messages_count = static_cast<std::uint16_t>(pending.size()),
            .type = MessageType::SingleBookUpdateMessage,
        };

        std::memcpy(buf.data(), &header, sizeof(header));

        std::size_t offset{sizeof(header)};
        for (const auto &msg: pending) {
            std::memcpy(buf.data() + offset, &msg, sizeof(msg));
            offset += sizeof(msg);
        }

        socket.send_to(asio::buffer(buf.data(), offset), endpoint);
        pending.clear();
    }

    std::uint32_t getSequenceNumber() const { return seq_num; }
    std::uint32_t getStockSequenceNumber() const { return stock_seq; }
private:
    void sendLevelUpdate(Price price, TradeSide side) {
        auto check = [&](const auto &levels) {
            auto it = levels.find(price);
            if (it == levels.end()) {
                pushUpdate(price, 0, side, PriceLevelAction::DELETE);
            } else {
                pushUpdate(price, levelTotal(it->second), side, PriceLevelAction::UPDATE);
            }
        };
        if (side == TradeSide::Buy) {
            check(book.getBids());
        } else {
            check(book.getAsks());
        }
    }

    void pushUpdate(Price price, Quantity qty, TradeSide side, PriceLevelAction action) {
        pending.push_back(SingleBookUpdateMessage{
            .ticker = ticker,
            .event_timestamp = getTimestampNs(),
            .price = price,
            .quantity = qty,
            .stock_sequence_number = stock_seq++,
            .side = side,
            .price_level = action,
        });
        if (pending.size() == MAX_MSGS_PER_PACKET) {
            flush();
        }
    }

    asio::ip::udp::socket &socket;
    asio::ip::udp::endpoint &endpoint;
    OrderBook book;
    std::vector<SingleBookUpdateMessage> pending;
    StockTicker ticker{};
    std::uint32_t seq_num{0};
    std::uint32_t stock_seq{0};
};

void help(char *argv[]) {
    std::println("You can download an aggregated NASDAQ itch file by calling ./download_nasdaq_itch.sh");
    std::println("To extract per-stock data, you can call: ");
    std::println("ExtractITCHPerTicker ./cache/12302019.NASDAQ_ITCH50 AAPL ./cache/AAPL_20191230.itch");
    std::println("And then call {} ./cache/AAPL_20191230.itch", argv[0]);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::println("Usage: {} <path_to_filtered_itch_file>", argv[0]);
        help(argv);
        return 1;
    }

    if (!std::filesystem::exists(argv[1])) {
        std::println("Itch file {} does not exist", argv[1]);
        help(argv);
        return 2;
    }

    asio::io_context io;
    asio::ip::udp::endpoint endpoint(asio::ip::make_address(MULTICAST_GROUP), MULTICAST_PORT);
    asio::ip::udp::socket socket(io, endpoint.protocol());

    Publisher pub{socket, endpoint};

    ITCH::ItchParser parser{argv[1]};
    parser.parseAll([&](auto &&result) {
        if (!result.has_value()) {
            return;
        }
        std::visit([&](auto &&msg) {
            pub.handle(msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            // sleep to simulate constat flow of data to subscribers
        }, *result);
    });

    pub.flush();
    std::println("Done. Sent {} packets, {} messages.", pub.getSequenceNumber(), pub.getStockSequenceNumber());
    return 0;
}
