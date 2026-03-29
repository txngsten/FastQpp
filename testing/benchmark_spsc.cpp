#include "../includes/SPSCQueue.h"
#include "../includes/atomic_queue/atomic_queue.h"
#include "../includes/mutex_queue.hpp"
#include "../includes/readerwriterqueue.h"
#include "../spsc_queue.hpp"
#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
#include <cstdint>
#include <folly/ProducerConsumerQueue.h>
#include <iostream>
#include <thread>

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
void run_spsc_benchmark(const std::string& name, Queue& q, PushFn push, PopFn pop, int cpu1,
                        int cpu2, int testTime) {
    std::cout << name << "\n";

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

        while (!stop.load(std::memory_order_relaxed)) {
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

        while (!stop.load(std::memory_order_relaxed)) {
            while (!push(q, payload)) {
            }
            payload++;
        }
    });

    while (ready.load(std::memory_order_acquire) < 2) {
    }

    start.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::seconds(testTime));
    stop.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    std::cout << "sum: " << sum << "\n";
    std::cout << ops / testTime << " ops/sec\n\n";
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

    constexpr std::size_t queueSize{1 << 16};

    fastq::SPSC<std::uint64_t> fastQ(queueSize);
    run_spsc_benchmark(
        "fastq::SPSC", fastQ, [](auto& q, auto v) { return q.push(v); },
        [](auto& q, auto& v) { return q.pop(v); }, cpu1, cpu2, testTime);

    moodycamel::ReaderWriterQueue<uint64_t> mcQ(queueSize);
    run_spsc_benchmark(
        "moodycamel::ReaderWriterQueue", mcQ, [](auto& q, auto v) { return q.enqueue(v); },
        [](auto& q, auto& out) { return q.try_dequeue(out); }, cpu1, cpu2, testTime);

    rigtorp::SPSCQueue<std::uint64_t> rigtorpQ(queueSize);
    run_spsc_benchmark(
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

    atomic_queue::AtomicQueue2<std::uint64_t, queueSize> atomicQ;
    run_spsc_benchmark(
        "atomic_queue::AtomicQueue2", atomicQ,
        [](auto& q, auto v) {
            q.push(v); // blocking
            return true;
        },
        [](auto& q, auto& out) { return q.try_pop(out); }, cpu1, cpu2, testTime);

    folly::ProducerConsumerQueue<std::uint64_t> follyQ(queueSize);
    run_spsc_benchmark(
        "folly::ProducerConsumerQueue", follyQ, [](auto& q, auto v) { return q.write(v); },
        [](auto& q, auto& out) { return q.read(out); }, cpu1, cpu2, testTime);

    boost::lockfree::spsc_queue<std::uint64_t> boostQ(queueSize);
    run_spsc_benchmark(
        "boost::lockfree::spsc_queue", boostQ, [](auto& q, auto v) { return q.push(v); },
        [](auto& q, auto& out) { return q.pop(out); }, cpu1, cpu2, testTime);

    MutexQueue<std::uint64_t> mutexQ;
    run_spsc_benchmark(
        "std::mutex std::queue", mutexQ, [](auto& q, auto v) { return q.write(v); },
        [](auto& q, auto& out) { return q.read(out); }, cpu1, cpu2, testTime);
}
