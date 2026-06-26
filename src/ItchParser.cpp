#include "ItchParser.hpp"


namespace ITCH {
    std::vector<Message> ItchParser::parseAll()  {
        std::vector<Message> messages{};

        std::uint16_t message_length{};
        [[likely]]
        while (readMessageLength(message_length)) {
            if constexpr (isLittleEndian()) {
                message_length = __builtin_bswap16(message_length);
            }

            [[likely]]
            if (auto msg = parseMessage(static_cast<MessageType>(f.get()), message_length); msg) {
                messages.push_back(*msg);
            }
        }
        return messages;
    }

    // skips messages that were not parsed properly
    std::expected<Message, std::string> ItchParser::parseNext() {
        std::uint16_t message_length{};
        [[likely]]
        while (readMessageLength(message_length)) {
            if constexpr (isLittleEndian()) {
                message_length = __builtin_bswap16(message_length);
            }

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

    template <typename E>
    constexpr bool isValidEnumValue([[maybe_unused]] E parsed_enum) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
            if (parsed_enum == [:e:]) {
                return true;
            }
        }
        return false;
    }

    std::expected<Message, std::string> ItchParser::parseMessage(MessageType type, std::uint16_t len) {
        template for (constexpr auto msg_type_info : getMessageTypes()) {
            if (type == [:msg_type_info:]::type) {
                using ResolvedMsgType = [:msg_type_info:];
                constexpr std::uint16_t expectedMsgSize = sizeof(ResolvedMsgType) + sizeof(MessageType);
                [[unlikely]] if (len != expectedMsgSize) {
                    ++failedReads;
                    std::int64_t skip_message_offset = static_cast<std::int64_t>(len) - static_cast<std::int64_t>(sizeof(MessageType));
                    f.seekg(skip_message_offset, std::ios::cur);
                    return std::unexpected(std::format("Resolved type was {} and expected length of {} bytes, but got {}. Forwarded read counter by {} bytes",
                        std::meta::identifier_of(msg_type_info), sizeof(ResolvedMsgType), expectedMsgSize, skip_message_offset));
                }

                ResolvedMsgType msg;
                [[unlikely]] if (!f.read(reinterpret_cast<char*>(&msg), sizeof(ResolvedMsgType))) {
                    ++failedReads;
                    return std::unexpected(std::format("Failed to read {} bytes to read {} type.", sizeof(ResolvedMsgType), std::meta::identifier_of(msg_type_info)));
                }

                template for (constexpr auto field_type_info : getMembers<ResolvedMsgType>()) {
                    using FieldType = std::remove_cvref_t<decltype(msg.[:field_type_info:])>;
                    if constexpr (std::is_enum_v<FieldType>) {
                        static_assert(sizeof(FieldType) == 1);
                        [[unlikely]] if (!isValidEnumValue(msg.[:field_type_info:])) {
                            ++failedReads;
                            return std::unexpected{std::format("Char '{}' cannot be parsed to field '{}' of type {} for {} message type.",
                                static_cast<char>(msg.[:field_type_info:]), std::meta::identifier_of(field_type_info), std::meta::identifier_of(^^FieldType), std::meta::identifier_of(msg_type_info))};
                        }
                    }
                }

                if constexpr (isLittleEndian()) {
                    swapMessageEndianness(msg);
                }

                ++successfulReads;
                return msg;
            }
        }

        ++failedReads;
        return std::unexpected("Unknown message type");
    }

}
