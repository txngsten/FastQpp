#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <numeric>
#include <thread>
#include <vector>

#include "../spsc_queue.hpp"

class SPSCQueueTest : public ::testing::Test {
  protected:
    static constexpr std::size_t CAP{2048};
    fastq::SPSC<int> q{CAP};
};
