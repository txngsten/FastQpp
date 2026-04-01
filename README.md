# FastQ++
A single producer single consumer lock-free blocking circular buffer queue for trivially copyable types.
It only supports two threads (a producer and consumer) that can NOT switch roles whilst the queue is active.
This queue also makes use of batch visibility for read and writes which increases throughput at the cost of latency.
This batch size is a template parameter and can be adjusted.
****

> [!WARNING]
> The producer thread needs exclusive ownership over push operations and the consumer thread the same for reads, this queue is designed with that assumption and if broken the behavior is undefined.
>
> Batch visibility means that you must use the flush method (shown bellow in [usage](#usage)).
> This ensures atomics are only touched every 1/BATCH_SIZE operations keeping contention low and throughput high at the cost of latency.
> Batch size is default set to 32 but can be taken as the secondary template argument.
>
> Sizing also must be powers of two since the use a bit-mask is used for wrap around behavior instead of costly modulo/remainder operations.

## Usage
Simply include the spsc_queue.hpp in your project, needs C++20 as concepts are used for template arguments.

```c++
#include "spsc_queue.hpp"

fastq::SPSC<int> fastQ(32); // Size must be non-zero power of two

bool push = fastQ.push(16); // push is a no-discard method, returns false if queue is full

fastQ.flush(); // Must be done at end of usage to get last items visible for popping

int num;
bool pop = fastQ.pop(num); // pop is also no-discard method, returns false if queue is empty
```

## Benchmark
Now it seems this queue is a bit a of a headache to use (it kind of is) but looking at the results I think it's quite worth it.

### Benchmark environment
- CPU: Apple M2 (10-core)
- OS: macOS (Tahoe 26.3.1)
- Compiler: clang++ (LLVM 21.1.8)
- Build: Release (-O3)
- C++ Standard: C++20
- Threads: 1 producer / 1 consumer (SPSC)
- Test Duration: 5 sec

Results were averaged over 10 total runs, with a batch size of 512.

### Throughput (ops/sec)

| Queue                        | Mean Ops/sec | Median Ops/sec |
|------------------------------|-------------:|---------------:|
| std::mutex std::queue        |   27,017,182 |     26,535,340 |
| Boost                        |   25,972,865 |     25,979,811 |
| Folly                        |   25,730,658 |     25,698,531 |
| Rigtorp                      |   27,021,942 |     26,669,206 |
| Atomic                       |   53,221,145 |     54,842,159 |
| Moody Camel                  |  420,688,516 |    420,114,676 |
| FastQ++                      |  460,050,530 |    460,203,054 |

### Third-Party Queue References

The following open-source queue implementations were used for benchmarking:

- [**Boost Lockfree Queue**](https://github.com/boostorg/lockfree)
- [**Facebook Folly Queues**](https://github.com/facebook/folly)
- [**Rigtorp SPSCQueue**](https://github.com/rigtorp/SPSCQueue)
- [**Moodycamel ReaderWriterQueue**](https://github.com/cameron314/readerwriterqueue)
- [**Atomic Queue**](https://github.com/max0x7ba/atomic_queue)

## TODO
- [ ] Proper unit tests  
- [ ] More robust / generic benchmarking  
- [ ] SPMC queue implementation  
- [ ] MPMC queue implementation  
- [ ] Extend the API  
- [ ] Platform-specific optimisations
