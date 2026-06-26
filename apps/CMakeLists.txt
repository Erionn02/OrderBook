include(CMakePrintHelpers)

add_app(OrderBookPublisher
        OrderBookPublisher.cpp
)

add_app(OrderBookSubscriber
        OrderBookSubscriber.cpp
)

add_app(ExtractITCHPerTicker
        ExtractITCHPerTicker.cpp
)