#include "../includes/SPSCQueue.h"
#include "../includes/atomic_queue/atomic_queue.h"
#include "../includes/mutex_queue.hpp"
#include "../spsc_queue.hpp"
#include <atomic>
#include <boost/lockfree/spsc_queue.hpp>
#include <chrono>
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

int main() {
    std::cout << "SPSC Queue Benchmark Test:\n\n";

    int cpu1{0};
    int cpu2{1};

    short testTime {5};

    constexpr std::size_t queueSize{1 << 16};

    // FastQ++ bench
    {
        std::cout << "fastq::SPSC\n";

        fastq::SPSC<u_int64_t> fastq(queueSize);

        std::atomic<bool> start{false};
        std::atomic<bool> stop{false};
        std::atomic<short> ready{};

        std::uint64_t ops{0};

        std::thread consumer([&] {
            pinThread(cpu1);

            std::uint64_t payload{0};
            std::uint64_t localOps{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                if (fastq.pop(payload)) {
                    localOps++;
                }
            }

            ops = localOps;
        });

        std::thread producer([&] {
            pinThread(cpu2);

            std::uint64_t payload{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                while (!fastq.push(payload)) {
                }
            }
        });

        while (ready.load(std::memory_order_acquire) < 2) {
        }

        start.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::seconds(testTime));
        stop.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        std::cout << ops / testTime << " ops/sec\n\n";
    }

    // Rigtorp bench
    {
        std::cout << "rigtorp::SPSC\n";

        rigtorp::SPSCQueue<u_int64_t> rigtorpQ(queueSize);

        std::atomic<bool> start{false};
        std::atomic<bool> stop{false};
        std::atomic<short> ready{};

        std::uint64_t ops{0};

        std::thread consumer([&] {
            pinThread(cpu1);

            std::uint64_t payload{0};
            std::uint64_t localOps{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                auto* ptr = rigtorpQ.front();
                if (ptr) {
                    rigtorpQ.pop();
                    localOps++;
                }
            }

            ops = localOps;
        });

        std::thread producer([&] {
            pinThread(cpu2);

            std::uint64_t payload{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                rigtorpQ.emplace(payload);
            }
        });

        while (ready.load(std::memory_order_acquire) < 2) {
        }

        start.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::seconds(testTime));
        stop.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        std::cout << ops / testTime << " ops/sec\n\n";
    }

    // Folly bench
    {
        std::cout << "Folly::ProducerConsumerQueue\n";

        folly::ProducerConsumerQueue<u_int64_t> follyQ(queueSize);

        std::atomic<bool> start{false};
        std::atomic<bool> stop{false};
        std::atomic<short> ready{};

        std::uint64_t ops{0};

        std::thread consumer([&] {
            pinThread(cpu1);

            std::uint64_t payload{0};
            std::uint64_t localOps{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                if (follyQ.read(payload)) {
                    localOps++;
                }
            }

            ops = localOps;
        });

        std::thread producer([&] {
            pinThread(cpu2);

            std::uint64_t payload{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                while (!follyQ.write(payload)) {
                }
            }
        });

        while (ready.load(std::memory_order_acquire) < 2) {
        }

        start.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::seconds(testTime));
        stop.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        std::cout << ops / testTime << " ops/sec\n\n";
    }

    // Atomic queue bench
    {
        std::cout << "atomic_queue::AtomicQueue2\n";

        atomic_queue::AtomicQueue2<u_int64_t, queueSize> atomicQ;

        std::atomic<bool> start{false};
        std::atomic<bool> stop{false};
        std::atomic<short> ready{};

        std::uint64_t ops{0};

        std::thread consumer([&] {
            pinThread(cpu1);

            std::uint64_t payload{0};
            std::uint64_t localOps{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                if (atomicQ.try_pop(payload)) {
                    localOps++;
                }
            }

            ops = localOps;
        });

        std::thread producer([&] {
            pinThread(cpu2);

            std::uint64_t payload{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                atomicQ.push(payload);
            }
        });

        while (ready.load(std::memory_order_acquire) < 2) {
        }

        start.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::seconds(testTime));
        stop.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        std::cout << ops / testTime << " ops/sec\n\n";
    }

    // Boost bench
    {
        std::cout << "boost::lockfree::spsc_queue\n";

        boost::lockfree::spsc_queue<u_int64_t> boostQ(queueSize);

        std::atomic<bool> start{false};
        std::atomic<bool> stop{false};
        std::atomic<short> ready{};

        std::uint64_t ops{0};

        std::thread consumer([&] {
            pinThread(cpu1);

            std::uint64_t payload{0};
            std::uint64_t localOps{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                if (boostQ.pop(payload)) {
                    localOps++;
                }
            }

            ops = localOps;
        });

        std::thread producer([&] {
            pinThread(cpu2);

            std::int64_t payload{0};

            ready.fetch_add(1, std::memory_order_release);

            while (!start.load(std::memory_order_acquire)) {
            }

            while (!stop.load(std::memory_order_relaxed)) {
                while (!boostQ.push(payload)) {
                }
            }
        });

        while (ready.load(std::memory_order_acquire) < 2) {
        }

        start.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::seconds(testTime));
        stop.store(true, std::memory_order_release);

        producer.join();
        consumer.join();

        std::cout << ops / testTime << " ops/sec\n\n";
    }

    // Mutex std::queue bench
    {

    }
}
