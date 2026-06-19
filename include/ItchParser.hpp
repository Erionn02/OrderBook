#pragma once
#include <chrono>

#include "ItchTypes.hpp"

#include <fstream>
#include <print>
#include <expected>
#include <chrono>
#include <filesystem>
#include <bit>

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
        ItchParser(const std::string& filename): f(filename, std::ios::binary) {
        }

        void parseAll() {
            auto start = std::chrono::high_resolution_clock::now();
            std::array<char, 2> buf{};
            [[likely]] while (f.read(buf.data(), buf.size())) {
                auto messageLength = __builtin_bswap16(*(std::uint16_t*)buf.data());
                [[unlikely]] if (messageLength == 0) {
                    break;
                }

                std::expected<Message, std::string_view> msg = parseMessage(static_cast<MessageType>(f.get()), messageLength);

                [[unlikely]] if (!msg) {
                    std::println("Failed to parse message, details: {}, successful reads: {}, failed: {}", msg.error(), successfulReads, failedReads);
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            auto total = successfulReads + failedReads;
            double speed = (double)total / (double)duration.count() / 1'000.0;
            std::println("Parsed {} messages in {}, speed: {} M/s", successfulReads + failedReads, duration, speed);
            std::println("{} successful reading", successfulReads);
            std::println("{} failed reading", failedReads);
        }

        std::expected<Message, std::string_view> parseMessage(MessageType type, std::uint16_t len) {

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
                    [[unlikely]] if (!f.read((char*)&msg, sizeof(ResolvedMsgType))) {
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



        std::size_t successfulReads{0};
        std::size_t failedReads{0};
        std::ifstream f;
    };

}
