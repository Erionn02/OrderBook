#include "ItchParser.hpp"

#include <print>
#include <chrono>

namespace ITCH {
    std::vector<Message> ItchParser::parseAll()  {
        std::vector<Message> messages{};
        auto start = std::chrono::high_resolution_clock::now();

        std::uint16_t message_length{};
        [[likely]]
        while (readMessageLength(message_length)) {
            message_length = __builtin_bswap16(message_length);

            [[likely]]
            if (auto msg = parseMessage(static_cast<MessageType>(f.get()), message_length); msg) {
                messages.push_back(*msg);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        auto total = successfulReads + failedReads;
        double speed = (double)total / (double)duration.count() / 1'000.0;
        std::println("Parsed {} messages in {}, speed: {} M/s", successfulReads + failedReads, duration, speed);
        std::println("{} successful reading", successfulReads);
        std::println("{} failed reading", failedReads);
        return messages;
    }

    // skips messages that were not parsed properly
    std::expected<Message, std::string_view> ItchParser::parseNext() {
        std::uint16_t message_length{};
        [[likely]] while (readMessageLength(message_length)) {
            message_length = __builtin_bswap16(message_length);

            [[likely]]
            if (auto msg = parseMessage(static_cast<MessageType>(f.get()), message_length); msg) {
                return msg;
            }
        }
        return std::unexpected("Stream end");
    }


    bool ItchParser::readMessageLength(std::uint16_t &message_length)  {
        return static_cast<bool>(f.read(reinterpret_cast<char*>(&message_length), sizeof(message_length))) && message_length != 0;
    }

    std::expected<Message, std::string_view> ItchParser::parseMessage(MessageType type, std::uint16_t len) {
        template for (constexpr auto msg_type_info : getMessageTypes()) {
            if (type == [:msg_type_info:]::type) {
                using ResolvedMsgType = [:msg_type_info:];
                constexpr std::uint16_t expectedMsgSize = sizeof(ResolvedMsgType) + sizeof(MessageType);
                if (len != expectedMsgSize) {
                    ++failedReads;
                    std::int64_t skip_message_offset = static_cast<std::int64_t>(len) - static_cast<std::int64_t>(sizeof(MessageType));
                    f.seekg(skip_message_offset, std::ios::cur);
                    return std::unexpected(std::format("Resolved type was {} and expected length of {} bytes, but got {}. Forwarded read counter by {} bytes",
                        std::meta::identifier_of(msg_type_info), sizeof(ResolvedMsgType), expectedMsgSize, skip_message_offset));
                }

                ResolvedMsgType msg{};
                [[unlikely]] if (!f.read(reinterpret_cast<char*>(&msg), sizeof(ResolvedMsgType))) {
                    ++failedReads;
                    return std::unexpected(std::format("Failed to read {} bytes to read {} type.", sizeof(ResolvedMsgType), std::meta::identifier_of(msg_type_info)));
                }

                if constexpr (isLittleEndian()) {
                    changeIntegerFieldsToLittleEndian(msg);
                }

                ++successfulReads;
                return msg;
            }
        }

        ++failedReads;
        return std::unexpected("Unknown message type");
    }

}
