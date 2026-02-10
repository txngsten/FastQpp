#include "src/mutex_queue.hpp"
#include "src/mutex_deque.hpp"
#include "third_party/concurrentqueue.h"
#include "third_party/atomic_queue/atomic_queue.h"
#include <folly/MPMCQueue.h>
#include <boost/lockfree/queue.hpp>
#include <tbb/concurrent_queue.h>

#include <sstream>
#include <iomanip>
#include <chrono>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>

struct SmallPayload {
    uint64_t value;
};

/* -------------------------------------- Queue Adapters -------------------------------------- */

template <typename Q>
struct QueueAdapter;

// Moody Camel
template <typename T>
struct QueueAdapter<moodycamel::ConcurrentQueue<T>> {
    static bool push(auto& q, const T& v) {
        return q.enqueue(v);
    }
    static bool pop(auto& q, T& v) {
        return q.try_dequeue(v);
    }
};

// Folly
template <typename T>
struct QueueAdapter<folly::MPMCQueue<T>> {
    static bool push(auto& q, const T& v) {
        return q.write(v);
    }
    static bool pop(auto& q, T& v) {
        return q.read(v);
    }
};

// Boost
template <typename T>
struct QueueAdapter<boost::lockfree::queue<T>> {
    static bool push(auto& q, const T& v) {
        return q.push(v);
    }
    static bool pop(auto& q, T& v) {
        return q.pop(v);
    }
};

// TBB
template <typename T>
struct QueueAdapter<tbb::concurrent_queue<T>> {
    static bool push(auto& q, const T& v) {
        q.push(v);
        return true;
    }
    static bool pop(auto& q, T& v) {
        return q.try_pop(v);
    }
};

// Atomic Queue
template <typename T, size_t N>
struct QueueAdapter<atomic_queue::AtomicQueue2<T, N>> {
    static bool push(auto& q, const T& v) {
        q.push(v);
        return true;
    }
    static bool pop(auto& q, T& v) {
        return q.try_pop(v);
    }
};

// MutexQ
template <typename T>
struct QueueAdapter<MutexQueue<T>> {
    static bool push(auto& q, const T& v) {
        q.write(v);
        return true;
    }
    static bool pop(auto& q, T& v) {
        return q.read(v);
    }
};

// MutexDQ
template <typename T>
struct QueueAdapter<MutexDeque<T>> {
    static bool push(auto& q, const T& v) {
        q.write(v);
        return true;
    }
    static bool pop(auto& q, T& v) {
        return q.read(v);
    }
};

/* -------------------------------------------------------------------------------------------- */



std::string makeTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

std::ofstream out("../data/benchmark_results/results_" + makeTimestamp() + ".txt");



void dequeueBenchmark() {
    constexpr size_t CAPACITY {65536};

    // Queue initialisation
    moodycamel::ConcurrentQueue<SmallPayload> moodyQ(CAPACITY);
    for (size_t i {}; i < CAPACITY; ++i) {
        moodyQ.enqueue(SmallPayload{static_cast<SmallPayload>(i)});
    }

    atomic_queue::AtomicQueue2<SmallPayload, CAPACITY> atomicQ;
    for (size_t i {}; i < CAPACITY; i++) {
        atomicQ.push(static_cast<SmallPayload>(i));
    }

    folly::MPMCQueue<SmallPayload> follyQ(CAPACITY);
    for (size_t i {}; i < CAPACITY; i++) {
        follyQ.write(static_cast<SmallPayload>(i));
    }

    boost::lockfree::queue<SmallPayload> boostQ(CAPACITY);
    for (size_t i {}; i < CAPACITY; i++) {
        boostQ.push(static_cast<SmallPayload>(i));
    }

    tbb::concurrent_queue<SmallPayload> tbbQ;
    for (size_t i {}; i < CAPACITY; i++) {
        tbbQ.push(static_cast<SmallPayload>(i));
    }

    MutexQueue<SmallPayload> mutexQ;
    for (size_t i {}; i < CAPACITY; i++) {
        mutexQ.write(static_cast<SmallPayload>(i));
    }

    MutexDeque<SmallPayload> mutexDQ;
    for (size_t i {}; i < CAPACITY; i++) {
        mutexDQ.write(static_cast<SmallPayload>(i));
    }

    std::vector<int> numThreads{{1, 2, 4, 8}};
    for (int N : numThreads) {
        std::atomic<int> ready {};
        std::atomic<bool> start {false};
        std::atomic<bool> stop {false};

        std::vector<std::thread> workers;




    }
}





int main() {

}