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

add_executable(ProtocolTest net/test_protocol.cpp)
target_include_directories(ProtocolTest PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(exchange net/exchange_main.cpp net/Gateway.cpp net/Journal.cpp net/Feed.cpp)
target_link_libraries(exchange PRIVATE OrderBookLib)
target_include_directories(exchange PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(participant_sim net/participant_sim.cpp bench/OrderGenerator.cpp)
target_link_libraries(participant_sim PRIVATE OrderBookLib)
target_include_directories(participant_sim PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(feed_subscriber net/feed_subscriber.cpp)
target_include_directories(feed_subscriber PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(feed_test net/feed_test.cpp net/Feed.cpp)
target_include_directories(feed_test PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(latency_probe net/latency_probe.cpp bench/OrderGenerator.cpp)
target_link_libraries(latency_probe OrderBookLib)
target_include_directories(latency_probe PRIVATE ${CMAKE_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(OrderBookTests)