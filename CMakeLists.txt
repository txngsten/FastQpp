cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0167 NEW)

project(FastQpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------- Dependencies ----------------
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(folly CONFIG REQUIRED)

# ---------------- Executable ----------------
add_executable(testbench testing/benchmark_spsc.cpp)

target_include_directories(testbench
  PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(
  testbench
  PRIVATE
    Boost::headers
    fmt::fmt
    Folly::folly
    Threads::Threads
)
