cmake_minimum_required(VERSION 3.30)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Default build type: Debug" FORCE)
endif()
message("CMAKE_VERSION: ${CMAKE_VERSION}")


project(OrderBook CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_SOURCE_DIR}/cmake/CMakeHelpers.cmake)
add_conan_lib(GTest)
add_conan_lib(benchmark)
add_conan_lib(asio)


set(PGO_MODE "OFF" CACHE STRING "PGO_MODE: OFF, GENERATE, USE")

set(PGO_DIR "${CMAKE_BINARY_DIR}/pgo_data")

if(PGO_MODE STREQUAL "GENERATE")
    message(STATUS "Build for generating PGO profile")
    add_compile_options(-O3 -fprofile-generate=${PGO_DIR})
    add_link_options(-fprofile-generate=${PGO_DIR})
elseif(PGO_MODE STREQUAL "USE")
    message(STATUS "Build for using PGO profile")
    add_compile_options(-O3 -fprofile-use=${PGO_DIR} -fprofile-correction -Wno-missing-profile)
    add_link_options(-fprofile-use=${PGO_DIR})
endif()

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
