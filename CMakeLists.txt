cmake_minimum_required(VERSION 3.22)
cmake_policy(SET CMP0167 NEW)

project(FastQpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- Homebrew root ----------------
# ensures Boost, TBB, Folly, etc. resolve from Homebrew
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
list(APPEND CMAKE_LIBRARY_PATH "/opt/homebrew/lib")
list(APPEND CMAKE_INCLUDE_PATH "/opt/homebrew/include")

# ---------------- Dependencies ----------------
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)
find_package(TBB REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(folly CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(gflags CONFIG REQUIRED)

# ---------------- Executable ----------------
add_executable(testbench test.cpp)

target_link_libraries(testbench PRIVATE
        Boost::headers
        TBB::tbb
        fmt::fmt
        Folly::folly
        glog::glog
        gflags
        Threads::Threads
)
