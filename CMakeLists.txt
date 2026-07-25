cmake_minimum_required(VERSION 3.28)
project(QuantOrderBook CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(OrderBookLib)
target_sources(OrderBookLib
        PUBLIC
        FILE_SET CXX_MODULES FILES
        OrderBookTypes.cppm
        OrderBookModels.cppm
        OrderBookEngine.cppm
)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

add_executable(OrderBookBench bench/bench_main.cpp bench/OrderGenerator.cpp)
target_link_libraries(OrderBookBench PRIVATE OrderBookLib)
target_include_directories(OrderBookBench PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(OrderBookTests test_orderbook.cpp)
target_link_libraries(OrderBookTests PRIVATE OrderBookLib gtest gtest_main)

include(GoogleTest)
gtest_discover_tests(OrderBookTests)