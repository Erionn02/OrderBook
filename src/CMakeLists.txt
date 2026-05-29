file(GLOB ALL_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_lib(OrderBookLib
        SOURCES ${ALL_SOURCES}
)