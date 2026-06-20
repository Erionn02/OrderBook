#pragma once
// Implemented according to https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf spec
// Example feeds can be found here https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH

#include <array>
#include <variant>
#include <meta>
#include <print>

namespace ITCH {
// packing structs to allow implementing protocol specs in an easy way
#pragma pack(push, 1)

    using Timestamp48_t = std::array<std::uint8_t, 6>; // According to documentation it's treated as nanoseconds since midnight
    using Stock_t = std::array<char, 8>;
    using TrackingNumber_t = std::uint16_t;
    using StockLocate_t = std::uint16_t;
    using Price_64 = std::size_t;
    using Price_32 = std::uint32_t;


    enum class MessageType : std::uint8_t {
        SystemEvent = 'S',
        StockDirectory = 'R',
        StockTradingAction = 'H',
        RegSHORestrictions = 'Y',
        MarketParticipantPosition = 'L',
        MWCBDeclineLevel = 'V',
        MWCBStatus = 'W',
        IPOQuotingPeriodUpdate = 'K',
        LULDAuctionCollar = 'J',
        OperationalHalt = 'h',
        AddOrder = 'A',
        AddOrderMPIDAttribution = 'F',
        OrderExecuted = 'E',
        OrderExecutedWithPrice = 'C',
        OrderCancel = 'X',
        OrderDelete = 'D',
        OrderReplace = 'U',
        NonCrossTrade = 'P',
        CrossTrade = 'Q',
        BrokenTrade = 'B',
        NetOrderImbalanceIndicator = 'I',
        RetailInterest = 'N',
        DirectListingWithCapitalRaisePriceDiscovery = 'O',
    };

    enum class OptionalBooleanChar : std::uint8_t {
        Y = 'Y',
        N = 'N',
        NotAvailable = ' '
    };

    enum class BooleanChar : std::uint8_t {
        Y = 'Y',
        N = 'N',
    };

    enum class Side : std::uint8_t {
        Buy = 'B',
        Sell = 'S'
    };

    inline namespace Messages {
        struct SystemEventMessage {
            static constexpr MessageType type{MessageType::SystemEvent};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;

            enum class EventCode: std::uint8_t {
                StartOfMessages = 'O',
                StartOfSystemHours = 'S',
                StartOfMarketHours = 'Q',
                EndOfMarketHours = 'M',
                EndOfSystemHours = 'E',
                EndOfMessage = 'C',
            } event_code;
        };


        struct StockDirectoryMessage {
            static constexpr MessageType type{MessageType::StockDirectory};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Stock_t stock;

            enum class MarketCategory : std::uint8_t {
                NasdaqGlobalSelectMarket = 'Q',
                NasdaqGlobalMarket = 'G',
                NasdaqCapitalMarket = 'S',
                Nyse = 'N',
                NyseAmerican = 'A',
                NyseArca = 'P',
                BatsZExchange = 'Z',
                InvestorsExchange = 'V',
                NotAvailable = ' '
            } market_category;

            enum class FinancialStatusIndicator : std::uint8_t {
                Deficient = 'D',
                Delinquent = 'E',
                Bankrupt = 'Q',
                Suspended = 'S',
                DeficientAndBankrupt = 'G',
                DeficientAndDelinquent = 'H',
                DelinquentAndBankrupt = 'J',
                DeficientDelinquentAndBankrupt = 'K',
                CreationsOrRedemptionsSuspendedForETP = 'C',
                Normal = 'N',
                NotAvailable = ' '
            } financial_status_indicator;

            std::uint32_t round_lot_size;

            OptionalBooleanChar round_lots_only;

            char issue_classification;
            std::array<char, 2>  issue_sub_type;

            enum class Authenticity: std::uint8_t {
                Production = 'P',
                Test = 'T'
            } authenticity;

            OptionalBooleanChar short_sale_threshold_indicator;
            OptionalBooleanChar ipo_flag;

            enum class LULDReferencePriceTier : std::uint8_t {
                Tier1 = '1',
                Tier2 = '2',
                NotAvailable = ' '
            } price_tier;

            OptionalBooleanChar etp_flag;
            std::uint32_t etp_leverage_factor;
            BooleanChar inverse_indicator;
        };


        struct StockTradingActionMessage {
            static constexpr MessageType type{MessageType::StockTradingAction};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Stock_t stock;
            enum class TradingState: std::uint8_t {
                Halted= 'H',
                Paused = 'P',
                QuotationOnly = 'Q',
                Trading = 'T'
            } trading_state;
            char reserved;
            std::array<char, 4> reason;
        };

        struct RegSHORestrictionsMessage {
            static constexpr MessageType type{MessageType::RegSHORestrictions};
            std::uint16_t locate_code;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Stock_t stock;
            enum class RegSHOAction : std::uint8_t {
                NoPriceTest = '0',
                IntraDayDropPriceRestriction = '1',
                Restriction = '2'
            } action;
        };

        struct MarketParticipantPositionMessage {
            static constexpr MessageType type{MessageType::MarketParticipantPosition};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::array<char, 4> mpid;
            Stock_t stock;
            BooleanChar primary_market_maker;
            enum class MarketMakerMode: std::uint8_t {
                Normal = 'N',
                Passive = 'P',
                Syndicate = 'S',
                PreSyndicate = 'R',
                Penalty = 'L'
            } mode;
            enum class State : std::uint8_t {
                Active = 'A',
                Excused = 'E',
                Withdrawn = 'W',
                Suspended = 'S',
                Deleted = 'D'
            } state;
        };

        struct MWCBDeclineLevelMessage {
            static constexpr MessageType type{MessageType::MWCBDeclineLevel};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Price_64 level1;
            Price_64 level2;
            Price_64 level3;
        };

        struct MWCBStatusMessage {
            static constexpr MessageType type{MessageType::MWCBStatus};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            enum class BreachedLevel: std::uint8_t {
                Level1 = '1',
                Level2 = '2',
                Level3 = '3'
            } breached_level;
        };

        struct IPOQuotingPeriodUpdateMessage {
            static constexpr MessageType type{MessageType::IPOQuotingPeriodUpdate};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Stock_t stock;
            int IPOQuotationReleaseTime;
            enum class IPOQuotationReleaseQualifier: std::uint8_t {
                AnticipatedReleaseTime = 'A',
                ReleasePostponedOrCanceled = 'C'
            } release_qualifier;
            Price_32 IPOPrice;
        };

        struct LULDAuctionCollarMessage {
            static constexpr MessageType type{MessageType::LULDAuctionCollar};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Stock_t stock;
            Price_32 reference_price;
            Price_32 upper_price;
            Price_32 lower_price;
            std::uint32_t extension;
        };

        struct OperationalHaltMessage {
            static constexpr MessageType type{MessageType::OperationalHalt};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Stock_t stock;
            enum class MarketCode : std::uint8_t {
                Nasdaq = 'Q',
                BX = 'B',
                PSX = 'X'
            } market_code;
            enum class OperationalHaltAction: std::uint8_t {
                Halted = 'H',
                TradingResumed = 'T'
            } halt_action;
        };

        struct AddOrderMessage {
            static constexpr MessageType type{MessageType::AddOrder};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t order_reference_number;
            Side side;
            std::uint32_t shares;
            Stock_t stock;
            Price_32 price;
        };

        struct AddOrderMPIDAttributionMessage {
            static constexpr MessageType type{MessageType::AddOrderMPIDAttribution};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t order_reference_number;
            Side side;
            std::uint32_t shares;
            Stock_t stock;
            Price_32 price;
            std::array<char, 4> attribution;
        };

        struct OrderExecutedMessage {
            static constexpr MessageType type{MessageType::OrderExecuted};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t order_reference_number;
            std::uint32_t executed_shares;
            std::size_t match_number;
        };

        struct OrderExecutedWithPriceMessage {
            static constexpr MessageType type{MessageType::OrderExecutedWithPrice};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t order_reference_number;
            std::uint32_t shares;
            std::size_t match_number;
            BooleanChar printable;
            Price_32 execution_price;
        };

        struct OrderCancelMessage {
            static constexpr MessageType type{MessageType::OrderCancel};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t order_reference_number;
            std::uint32_t cancelled_shares;
        };

        struct OrderDeleteMessage {
            static constexpr MessageType type{MessageType::OrderDelete};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t order_reference_number;
        };

        struct OrderReplaceMessage {
            static constexpr MessageType type{MessageType::OrderReplace};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t original_order_reference_number;
            std::size_t new_order_reference_number;
            std::uint32_t new_quantity;
            Price_32 new_price;
        };

        struct NonCrossTradeMessage {
            static constexpr MessageType type{MessageType::NonCrossTrade};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t order_reference_number;
            Side side;
            std::uint32_t shares;
            Stock_t stock;
            Price_32 price;
            std::size_t match_number;
        };

        struct CrossTradeMessage {
            static constexpr MessageType type{MessageType::CrossTrade};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t shares;
            Stock_t stock;
            Price_32 cross_price;
            std::size_t match_number;
            enum class CrossType : std::uint8_t {
                Opening = 'O',
                Closing = 'C',
                Halted = 'H'
            } cross_type;
        };

        struct BrokenTradeMessage {
            static constexpr MessageType type{MessageType::BrokenTrade};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t match_number;
        };

        struct NetOrderImbalanceIndicatorMessage {
            static constexpr MessageType type{MessageType::NetOrderImbalanceIndicator};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            std::size_t paired_shares;
            std::size_t imbalance_shares;
            enum class ImbalanceDirection : std::uint8_t {
                Buy = 'B',
                Sell = 'S',
                NoImbalance = 'N',
                InsufficientToCalculate = 'O',
                Paused = 'P'
            } imbalance_direction;
            Stock_t stock;
            Price_32 far_price;
            Price_32 near_price;
            Price_32 current_reference_price;
            enum class CrossType : std::uint8_t {
                Opening = 'O',
                Closing = 'C',
                Halted = 'H',
                ExtendedTradingClose = 'A'
            } cross_type;

            enum class PriceVariationIndicator : std::uint8_t {
                LessThan1p = 'L',
                p1 = '1',
                p2 = '2',
                p3 = '3',
                p4 = '4',
                p5 = '5',
                p6 = '6',
                p7 = '7',
                p8 = '8',
                p9 = '9',
                A = 'A',
                B = 'B',
                C = 'C',
                CannotBeCalculated = ' '
            } price_variation_indicator;
        };

        struct RetailInterestMessage {
            static constexpr MessageType type{MessageType::RetailInterest};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Stock_t stock;
            enum class InterestFlag : std::uint8_t {
                OrdersAvailableOnBuySide = 'B',
                OrdersAvailableOnSellSide = 'S',
                OrdersAvailableOnBothSides = 'A',
                NoOrdersAvailable = 'N'
            } interest_flag;
        };

        struct DirectListingWithCapitalRaisePriceDiscoveryMessage {
            static constexpr MessageType type{MessageType::DirectListingWithCapitalRaisePriceDiscovery};
            StockLocate_t stock_locate;
            TrackingNumber_t tracking_number;
            Timestamp48_t timestamp;
            Stock_t stock;
            BooleanChar open_eligibility_status;
            Price_32 minimum_allowable_price;
            Price_32 maximum_allowable_price;
            Price_32 near_execution_price;
            std::size_t near_execution_time;
            Price_32 lower_price_range_collar;
            Price_32 upper_price_range_collar;
        };

    } // namespace Messages

    consteval auto getMessageTypes() {
        std::vector<std::meta::info> types;

        constexpr auto messages_ns = ^^ITCH::Messages;
        for (auto member : std::meta::members_of(messages_ns, std::meta::access_context::current())) {
            if (std::meta::is_type(member) && std::meta::has_identifier(member) && std::meta::identifier_of(member).ends_with("Message")) {

                for (auto field : std::meta::static_data_members_of(member, std::meta::access_context::current())) {
                    if (std::meta::has_identifier(field) && std::meta::identifier_of(field) == "type") {
                        types.push_back(member);
                        break;
                    }
                }
            }
        }

        return std::define_static_array(types);
    }

    consteval auto getMessageTypesVariant() {
        return std::meta::substitute(^^std::variant, getMessageTypes());
    }

    using Message = [: getMessageTypesVariant() :];


    static_assert(std::__is_specialization_of<Message, std::variant>);
    static_assert(std::variant_size_v<Message> == std::meta::enumerators_of(^^MessageType).size());


    template<typename T>
    consteval decltype(auto) getMembers() {
        return std::define_static_array(std::meta::nonstatic_data_members_of(^^std::remove_cvref_t<T>, std::meta::access_context::current()));
    }

    template <typename E>
    constexpr std::string_view enumToString(E value) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
            if (value == [:e:]) {
                return std::meta::identifier_of(e);
            }
        }
        return "<unknown>";
    }

    void inline printMessage(const Message& message) {
        std::visit([]<typename T>(T&& msg) {
            using CleanType = std::remove_cvref_t<T>;
            constexpr auto real_type_info = std::meta::dealias(^^CleanType);
            std::print("{} \n", std::meta::identifier_of(real_type_info));

            template for (constexpr auto member : getMembers<CleanType>()) {
                using real_type = std::remove_cvref_t<decltype(msg.[:member:])>;
                if constexpr (std::is_enum_v<real_type>) {
                    std::print("  - {}: {}\n", std::meta::identifier_of(member), ITCH::enumToString(msg.[:member:]));
                } else {
                    std::println("  - {}: {}", std::meta::identifier_of(member), msg.[:member:]);
                }
            }
        }, message);
    }


} // namespace ITCH


#pragma pack(pop)
