# FastQ++
A collection of low-latency thread-safe lock-free queues consisting of a single producer single consumer (SPSC), single producer multi-consumer (SPMC), and a multi-producer multi-consumer (MPMC) variants.
These queues are all implemented with a shared memory ring buffer model and have been benchmarked against a variety of other concurrent queues listed as:
- [MoodyCamel Queue](https://github.com/cameron314/concurrentqueue)
- [Atomic Queue](https://github.com/max0x7ba/atomic_queue)
- [Folly MPMC Queue](https://github.com/facebook/folly)
- [Boost Lock-free Queue](https://www.boost.org/doc/libs/latest/doc/html/lockfree.html)
- [TBB Concurrent Queue](https://www.intel.com/content/www/us/en/docs/onetbb/developer-guide-api-reference/2021-9/concurrent-queue-classes.html)

