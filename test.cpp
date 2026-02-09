#include "src/mutex_queue.hpp"
#include "third_party/concurrentqueue.h"
#include <boost/lockfree/queue.hpp>
#include <tbb/concurrent_queue.h>
#include "third_party/atomic_queue.h"
#include <folly/MPMCQueue.h>

#include <sstream>
#include <iomanip>
#include <chrono>
#include <string>
#include <fstream>
#include <iostream>

std::string makeTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

std::ofstream out("../data/benchmark_results/results_" + makeTimestamp() + ".txt");







int main() {

}