cmake_minimum_required(VERSION 3.30)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Default build type: Debug" FORCE)
endif()
message("CMAKE_VERSION: ${CMAKE_VERSION}")

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b") # hash for cmake 4.3, if you have different then set an adequate value

project(OrderBook CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_MODULE_STD ON)

include(${CMAKE_SOURCE_DIR}/cmake/CMakeHelpers.cmake)
add_conan_lib(GTest)
add_conan_lib(benchmark)



# Turn on with 'cmake -Dcoverage=ON ..'.
option(coverage "Build for coverage report." OFF)

if (CMAKE_COMPILER_IS_GNUCXX AND coverage)
    message("Coverage report ENABLED")

    include(cmake/CodeCoverage.cmake)
    append_coverage_compiler_flags()
    setup_target_for_coverage_gcovr_html(NAME unit_tests_coverage
            EXECUTABLE cd tests/unit_tests && ctest --output-on-failure
            EXCLUDE ${CMAKE_SOURCE_DIR}/tests/* ${CMAKE_SOURCE_DIR}/apps/* ${CMAKE_SOURCE_DIR}/src/main.cpp)

    setup_target_for_coverage_gcovr_html(NAME integration_tests_coverage
            EXECUTABLE cd tests/integration_tests && ctest --output-on-failure
            EXCLUDE ${CMAKE_SOURCE_DIR}/tests/* ${CMAKE_SOURCE_DIR}/apps/* ${CMAKE_SOURCE_DIR}/src/main.cpp)

else ()
    message("Coverage report disabled")
endif ()

add_subdirectory(src)
add_subdirectory(apps)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    enable_testing()
    include(CTest)
    include(GoogleTest)
    add_subdirectory(tests)
else ()
    message("RELEASE mode - testing disabled")
endif ()

add_app(OrderBookBenchmark
        tests/OrderBookBenchmark.cpp
)

add_app(RealWorldDataOrderBookBenchmark
        tests/RealWorldDataOrderBookBenchmark.cpp
)

add_app(ITCHParserBenchmark
        tests/ITCHParserBenchmark.cpp
)
