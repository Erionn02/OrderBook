#pragma once
#include "ItchTypes.hpp"

#include <fstream>
#include <expected>
#include <filesystem>
#include <bit>
#include <concepts>

namespace ITCH {
    consteval bool isLittleEndian() {
        return std::endian::native == std::endian::little;
    }

    template<typename T>
    T swapEndianness(const char* data) {
        std::array<char, sizeof(T)> swapped{};
        for (size_t i = 0; i < sizeof(T); ++i) {
            swapped[sizeof(T) - 1 - i] = data[i];
        }
        return *reinterpret_cast<T*>(swapped.data());
    }

    class ItchParser {
    public:
        ItchParser(const std::string& filename): f(filename, std::ios::binary) {}

        std::vector<Message> parseAll();
        std::expected<Message, std::string_view> parseNext();
        bool readMessageLength(std::uint16_t& message_length);
        std::expected<Message, std::string_view> parseMessage(MessageType type, std::uint16_t len);

        template<typename MessageHandler>
        requires std::invocable<MessageHandler, std::expected<MessageType, std::string_view>>
        void parseAll(MessageHandler&& handler) {
            std::uint16_t message_length{};
            [[likely]]
            while (readMessageLength(message_length)) {
                message_length = __builtin_bswap16(message_length);
                handler(parseMessage(static_cast<MessageType>(f.get()), message_length));
            }
        }

        template<typename T>
        void changeIntegerFieldsToLittleEndian(T& msg) {
            template for (constexpr auto field : getMembers<T>()) {
                using fieldType = decltype(msg.[:field:]);

                // TODO: that also includes chars/booleans
                if constexpr (std::is_integral_v<fieldType> || std::is_same_v<fieldType, Timestamp48_t>) {
                    msg.[:field:] = swapEndianness<fieldType>(reinterpret_cast<const char*>(&msg.[:field:]));
                }
            }
        }

    private:
        std::size_t successfulReads{0};
        std::size_t failedReads{0};
        std::ifstream f;
    };

}
