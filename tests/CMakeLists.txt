package_add_test(OrderBookTests
        SOURCES OrderBookTests.cpp
        DEPENDS OrderBookLib
        )

package_add_test(PMATests
        SOURCES PackedMemoryArrayTests.cpp
)
