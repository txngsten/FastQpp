cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0167 NEW)
project(FastQpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_PREFIX_PATH "/opt/homebrew" ${CMAKE_PREFIX_PATH})

add_library(FastQpp INTERFACE)

target_include_directories(FastQpp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Boost REQUIRED)
find_package(TBB REQUIRED)
find_package(folly REQUIRED)

add_executable(testbench test.cpp)

target_link_libraries(testbench PRIVATE
        FastQpp
        Boost::boost
        TBB::tbb
        folly::folly
)