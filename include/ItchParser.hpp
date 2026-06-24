#pragma once
#include "ItchBase.hpp"

#include <fstream>
#include <expected>
#include <filesystem>
#include <concepts>
#include <bit>

namespace ITCH {
    consteval bool isLittleEndian() {
        return std::endian::native == std::endian::little;
    }

    class ItchParser {
    public:
        ItchParser(const std::string& filename): f(filename, std::ios::binary) {}

        std::vector<Message> parseAll();
        std::expected<Message, std::string> parseNext();
        bool readMessageLength(std::uint16_t& message_length);
        std::expected<Message, std::string> parseMessage(MessageType type, std::uint16_t len);

        template<typename MessageHandler>
        requires std::invocable<MessageHandler, std::expected<Message, std::string>>
        void parseAll(MessageHandler&& handler) {
            std::uint16_t message_length{};
            [[likely]] while (readMessageLength(message_length)) {
                if constexpr (isLittleEndian()) {
                    message_length = __builtin_bswap16(message_length);
                }

                handler(parseMessage(static_cast<MessageType>(f.get()), message_length));
            }
        }

        std::size_t getFailedReads() const { return failedReads; }
        std::size_t getSuccessfulReads() const { return successfulReads; }
    private:
        std::size_t successfulReads{0};
        std::size_t failedReads{0};
        std::ifstream f;
    };

}
