#pragma once

#include "BaseTypes.hpp"
#include "MulticastConfig.hpp"

#include <array>
#include <cstdint>

namespace BookUpdateProtocol {

enum class PriceLevelAction : std::uint8_t {
    UPDATE,
    DELETE
};

using StockTicker = std::array<char, 8>;

#pragma pack(push, 1)

inline namespace v1 {
    static constexpr std::uint8_t CURRENT_VERSION{0x01};

    enum class MessageType : std::uint8_t {
        SingleBookUpdateMessage,
    };

    struct SingleBookUpdateMessage {
        static constexpr MessageType type{MessageType::SingleBookUpdateMessage};
        StockTicker ticker;
        std::size_t event_timestamp;
        Price price;
        Quantity quantity;
        std::uint32_t stock_sequence_number;
        TradeSide side;
        PriceLevelAction price_level;
    };

    struct Header {
        std::size_t send_time;
        std::uint32_t sequence_number;
        std::uint16_t messages_count;
        MessageType type;
        std::uint8_t version{CURRENT_VERSION};
    };

    inline constexpr std::size_t MAX_MSGS_PER_PACKET = (MAX_DATAGRAM_PAYLOAD - sizeof(Header)) / sizeof(SingleBookUpdateMessage);
}

#pragma pack(pop)

} // namespace BookUpdateProtocol
