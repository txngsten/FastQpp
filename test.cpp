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
#include <chrono>
#include <cstdint>
#include <cstddef>


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

template <typename Queue>
uint64_t runDequeBenchmark(Queue& q, int numThreads) {
    std::atomic<int> ready {0};
    std::atomic<bool> start {false};
    std::atomic<bool> stop {false};

    std::atomic<uint64_t> totalOps {0};

    std::vector<std::thread> workers;
    for (int i {}; i < numThreads; i++) {
        workers.emplace_back([&] {
            SmallPayload v {};
            uint64_t localOps {};


            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            while (!stop.load(std::memory_order_relaxed)) {
                if (QueueAdapter<Queue>::pop(q, v)) {
                    localOps++;
                }
            }
            totalOps.fetch_add(localOps, std::memory_order_relaxed);
        });
    }

    while (ready.load(std::memory_order_acquire) < numThreads) {
        std::this_thread::yield();
    }

    start.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true, std::memory_order_release);

    for (auto& t : workers) {
        t.join();
    }

    return totalOps.load();
}

template <typename Queue>
void populateQueue(Queue& q, size_t capacity) {
    for (size_t i {}; i < capacity; i++) {
        SmallPayload v {static_cast<uint64_t>(i)};
        QueueAdapter<Queue>::push(q, v);
    }
}

std::string makeTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

// std::ofstream out("../data/benchmark_results/results_" + makeTimestamp() + ".txt");


void dequeueBenchmark() {
    constexpr size_t CAPACITY = 10'000'000;

    std::vector<int> numThreads{{1, 2, 4, 8}};
    for (int N : numThreads) {
        {
            // MoodyCamel
            moodycamel::ConcurrentQueue<SmallPayload> moodyQ(CAPACITY);
            populateQueue(moodyQ, CAPACITY);
            auto ops = runDequeBenchmark(moodyQ, N);
            std::cout << "MoodyCamel " << N << " threads, total ops: " << ops << '\n';
        }

        {
            // AtomicQueue
            auto atomicQ = std::make_unique<atomic_queue::AtomicQueue2<SmallPayload, CAPACITY>>();
            populateQueue(*atomicQ, CAPACITY);
            auto ops = runDequeBenchmark(*atomicQ, N);
            std::cout << "AtomicQueue " << N << " threads, total ops: " << ops << '\n';
        }

        {
            // Folly
            folly::MPMCQueue<SmallPayload> follyQ(CAPACITY);
            populateQueue(follyQ, CAPACITY);
            auto ops = runDequeBenchmark(follyQ, N);
            std::cout << "Folly " << N << " threads, total ops: " << ops << '\n';
        }

        {
            // Boost
            boost::lockfree::queue<SmallPayload> boostQ(CAPACITY);
            populateQueue(boostQ, CAPACITY);
            auto ops = runDequeBenchmark(boostQ, N);
            std::cout << "Boost " << N << " threads, total ops: " << ops << '\n';
        }

        {
            // TBB
            tbb::concurrent_queue<SmallPayload> tbbQ;
            populateQueue(tbbQ, CAPACITY);
            auto ops = runDequeBenchmark(tbbQ, N);
            std::cout << "TBB " << N << " threads, total ops: " << ops << '\n';
        }

        {
            // std::mutex Queue
            MutexQueue<SmallPayload> mutexQ;
            populateQueue(mutexQ, CAPACITY);
            auto ops = runDequeBenchmark(mutexQ, N);
            std::cout << "Mutex Queue " << N << " threads, total ops: " << ops << '\n';
        }

        {
            // std::mutex Deque
            MutexDeque<SmallPayload> mutexDQ;
            populateQueue(mutexDQ, CAPACITY);
            auto ops = runDequeBenchmark(mutexDQ, N);
            std::cout << "Mutex Deque " << N << " threads, total ops: " << ops << '\n';
        }
    }
}

int main() {
    dequeueBenchmark();
}