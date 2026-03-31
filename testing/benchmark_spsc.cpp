#include "../includes/SPSCQueue.h"
#include "../includes/atomic_queue/atomic_queue.h"
#include "../includes/mutex_queue.hpp"
#include "../includes/readerwriterqueue.h"
#include "../spsc_queue.hpp"
#include <algorithm>
#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdint>
#include <folly/ProducerConsumerQueue.h>
#include <iostream>
#include <vector>
#include <numeric>
#include <thread>
#include <unordered_map>

#if defined(__APPLE__)
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>

#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>

#elif defined(_WIN32)
#include <windows.h>
#endif

inline void pinThread(int core_id) {
    if (core_id < 0)
        return;

#if defined(__APPLE__)

    thread_affinity_policy_data_t policy = {core_id};
    thread_port_t mach_thread = pthread_mach_thread_np(pthread_self());

    thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);

#elif defined(__linux__)

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    const int max_cores = std::thread::hardware_concurrency();
    core_id = core_id % max_cores;

    CPU_SET(core_id, &cpuset);

    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

#elif defined(_WIN32)

    const int max_cores = std::thread::hardware_concurrency();
    core_id = core_id % max_cores;

    DWORD_PTR mask = (1ull << core_id);
    SetThreadAffinityMask(GetCurrentThread(), mask);

#else

    // fallback: no-op
    (void)core_id;

#endif
}

template <typename Queue, typename PushFn, typename PopFn>
std::vector<std::uint64_t> run_spsc_benchmark(const std::string& name, Queue& q, PushFn push,
                                              PopFn pop, int cpu1, int cpu2, int testTime) {
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<int> ready{0};

    std::uint64_t ops{0};
    std::uint64_t sum{0};

    std::thread consumer([&] {
        pinThread(cpu1);

        std::uint64_t payload{0};
        std::uint64_t localOps{0};
        std::uint64_t localSum{0};

        ready.fetch_add(1, std::memory_order_release);

        while (!start.load(std::memory_order_acquire)) {
        }

        while (!stop.load(std::memory_order_acquire)) {
            if (pop(q, payload)) {
                localSum += payload;
                localOps++;
            }
        }

        // drain (not counted)
        while (pop(q, payload)) {
            localSum += payload;
        }

        ops = localOps;
        sum = localSum;
    });

    std::thread producer([&] {
        pinThread(cpu2);

        std::uint64_t payload{0};

        ready.fetch_add(1, std::memory_order_release);

        while (!start.load(std::memory_order_acquire)) {
        }

        while (!stop.load(std::memory_order_acquire)) {
            while (!push(q, payload)) {
                std::this_thread::yield();
            }
            payload++;
        }
    });

    while (ready.load(std::memory_order_acquire) < 2) {
    }
    
    auto startTime {std::chrono::steady_clock::now()};
    start.store(true, std::memory_order_release);
    
    std::this_thread::sleep_for(std::chrono::seconds(testTime));
    
    stop.store(true, std::memory_order_release);
    auto endTime{std::chrono::steady_clock::now()};

    producer.join();
    consumer.join();

    double seconds {std::chrono::duration<double>(endTime - startTime).count()};
    double opsPerSec {ops / seconds};

    return {static_cast<std::uint64_t>(opsPerSec), sum};
}

void test_spsc_correctness() {
    constexpr std::size_t N = 10'000'000;
    fastq::SPSC<uint64_t> q(1 << 16);

    std::atomic<bool> start{false};
    std::atomic<bool> done{false};

    std::thread producer([&] {
        while (!start.load(std::memory_order_acquire)) {
        }

        for (uint64_t i = 0; i < N; ++i) {
            while (!q.push(i)) {
            }
        }

        done.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
        uint64_t expected = 0;
        uint64_t value;

        start.store(true, std::memory_order_release);

        while (!done.load(std::memory_order_acquire) || !q.empty()) {
            if (q.pop(value)) {
                if (value != expected) {
                    std::cerr << "ERROR: expected " << expected << " got " << value << "\n";
                    std::abort();
                }
                ++expected;
            }
        }

        q.flush();
        while (q.pop(value)) {
            if (value != expected) {
                std::cerr << "ERROR: expected " << expected << " got " << value << '\n';
                std::abort();
            }
            ++expected;
        }

        if (expected != N) {
            std::cerr << "ERROR: missing values. got " << expected << " expected " << N << "\n";
            std::abort();
        }

        std::cout << "PASS: correctness test\n";
    });

    producer.join();
    consumer.join();
}

int main() {
    test_spsc_correctness();

    std::cout << "SPSC Queue Benchmark Test:\n\n";

    int cpu1{0};
    int cpu2{1};

    short testTime{2};

    constexpr std::size_t queueSize{1 << 17};

    std::unordered_map<std::string, std::vector<std::uint64_t>> results;

    for (int i{}; i < 10; i++) {
        std::vector<std::uint64_t> result;

        fastq::SPSC<std::uint64_t, 512> fastQ(queueSize);
        result = run_spsc_benchmark(
            "fastq::SPSC", fastQ, [](auto& q, auto v) { return q.push(v); },
            [](auto& q, auto& v) { return q.pop(v); }, cpu1, cpu2, testTime);
        results["fastq"].push_back(result[0]);

        moodycamel::ReaderWriterQueue<uint64_t> mcQ(queueSize);
        result = run_spsc_benchmark(
            "moodycamel::ReaderWriterQueue", mcQ, [](auto& q, auto v) { return q.try_enqueue(v); },
            [](auto& q, auto& out) { return q.try_dequeue(out); }, cpu1, cpu2, testTime);
        results["moody"].push_back(result[0]);

        rigtorp::SPSCQueue<std::uint64_t> rigtorpQ(queueSize);
        result = run_spsc_benchmark(
            "rigtorp::SPSC", rigtorpQ,
            [](auto& q, auto v) {
                q.emplace(v);
                return true; // never fails
            },
            [](auto& q, auto& out) {
                auto* ptr = q.front();
                if (!ptr)
                    return false;
                out = *ptr;
                q.pop();
                return true;
            },
            cpu1, cpu2, testTime);
        results["rigtorp"].push_back(result[0]);

        atomic_queue::AtomicQueue2<std::uint64_t, queueSize> atomicQ;
        result = run_spsc_benchmark(
            "atomic_queue::AtomicQueue2", atomicQ,
            [](auto& q, auto v) {
                q.push(v); // blocking
                return true;
            },
            [](auto& q, auto& out) { return q.try_pop(out); }, cpu1, cpu2, testTime);
        results["atomic"].push_back(result[0]);

        folly::ProducerConsumerQueue<std::uint64_t> follyQ(queueSize);
        result = run_spsc_benchmark(
            "folly::ProducerConsumerQueue", follyQ, [](auto& q, auto v) { return q.write(v); },
            [](auto& q, auto& out) { return q.read(out); }, cpu1, cpu2, testTime);
        results["folly"].push_back(result[0]);

        boost::lockfree::spsc_queue<std::uint64_t> boostQ(queueSize);
        result = run_spsc_benchmark(
            "boost::lockfree::spsc_queue", boostQ, [](auto& q, auto v) { return q.push(v); },
            [](auto& q, auto& out) { return q.pop(out); }, cpu1, cpu2, testTime);
        results["boost"].push_back(result[0]);

        MutexQueue<std::uint64_t> mutexQ;
        result = run_spsc_benchmark(
            "std::mutex std::queue", mutexQ, [](auto& q, auto v) { return q.write(v); },
            [](auto& q, auto& out) { return q.read(out); }, cpu1, cpu2, testTime);
        results["mutex"].push_back(result[0]);
    }

    for (auto& [queue, ops] : results) {
        std::cout << queue << " results\n";

        std::sort(ops.begin(), ops.end());
        std::uint64_t totalOps{std::accumulate(ops.begin(), ops.end(), 0ULL)};
        std::uint64_t mean{totalOps / 10};
        std::uint64_t median{(ops[4] + ops[5]) / 2};
        std::uint64_t maxOps{*std::max_element(ops.begin(), ops.end())};
        std::uint64_t minOps{*std::min_element(ops.begin(), ops.end())};

        std::cout << "Mean ops/sec: " << mean << '\n';
        std::cout << "Median ops/sec: " << median << '\n';
        std::cout << "Max ops/sec: " << maxOps << '\n';
        std::cout << "Min ops/sec: " << minOps << "\n\n";
    }
}
