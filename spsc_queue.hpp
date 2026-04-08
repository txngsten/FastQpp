#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace fastq {
    template <typename T, std::size_t PublishBatch = 32, typename Allocator = std::allocator<T>>
        requires std::is_trivially_copyable_v<T>
    class SPSC {
      private:
        T* buffer_;
        Allocator alloc_;

        struct alignas(std::hardware_destructive_interference_size) Writer {
            std::atomic<std::size_t> value_{0};
        };
        struct alignas(std::hardware_destructive_interference_size) Reader {
            std::atomic<std::size_t> value_{0};
        };

        Writer writer_;
        Reader reader_;

        std::size_t mask_;
        std::size_t capacity_;

        struct alignas(std::hardware_destructive_interference_size) ProducerState {
            std::size_t localWriter_{0};
            std::size_t cachedReader_{0};
            std::size_t lastPublished_{0};
        };
        struct alignas(std::hardware_destructive_interference_size) ConsumerState {
            std::size_t localReader_{0};
            std::size_t cachedWriter_{0};
            std::size_t lastPublished_{0};
        };

        ProducerState producer_;
        ConsumerState consumer_;

        void publish_writer() noexcept {
            writer_.value_.store(producer_.localWriter_, std::memory_order_release);
            producer_.lastPublished_ = producer_.localWriter_;
        }

        void publish_reader() noexcept {
            reader_.value_.store(consumer_.localReader_, std::memory_order_release);
            consumer_.lastPublished_ = consumer_.localReader_;
        }

      public:
        explicit SPSC(std::size_t capacity)
            : buffer_(nullptr), alloc_(), mask_(0), capacity_(capacity) {
            if (capacity_ == 0 || (capacity_ & (capacity_ - 1)) != 0) {
                throw std::invalid_argument("Capacity must be non-zero power of 2");
            }

            mask_ = capacity_ - 1;
            buffer_ = std::allocator_traits<Allocator>::allocate(alloc_, capacity_);
        }

        // Non-copyable and non-moveable
        SPSC(const SPSC&) = delete;
        SPSC& operator=(const SPSC&) = delete;
        SPSC(SPSC&& other) = delete;
        SPSC& operator=(SPSC&& other) = delete;

        ~SPSC() {
            if (buffer_) {
                std::allocator_traits<Allocator>::deallocate(alloc_, buffer_, capacity_);
            }
        }

        // Producer thread must have exclusive control of the push method
        [[nodiscard]] bool push(T data) noexcept {
            if (producer_.localWriter_ - producer_.cachedReader_ == capacity_) {
                producer_.cachedReader_ = reader_.value_.load(std::memory_order_acquire);

                if (producer_.localWriter_ - producer_.cachedReader_ == capacity_) {
                    publish_writer();
                    return false;
                }
            }

            buffer_[producer_.localWriter_ & mask_] = data;
            ++producer_.localWriter_;

            if (producer_.localWriter_ - producer_.lastPublished_ >= PublishBatch) {
                publish_writer();
            }

            return true;
        }

        // Consumer thread must have exclusive control of the pop method
        [[nodiscard]] bool pop(T& data) noexcept {
            if (consumer_.localReader_ == consumer_.cachedWriter_) {
                consumer_.cachedWriter_ = writer_.value_.load(std::memory_order_acquire);

                if (consumer_.localReader_ == consumer_.cachedWriter_) {
                    publish_reader();
                    return false;
                }
            }

            data = buffer_[consumer_.localReader_ & mask_];
            ++consumer_.localReader_;

            if (consumer_.localReader_ - consumer_.lastPublished_ >= PublishBatch) {
                publish_reader();
            }

            return true;
        }

        // MUST be used at end of queue usage to ensure correct visibility
        void flush() noexcept {
            publish_reader();
            publish_writer();
        }

        // If queue is active this is NOT 100% accurate measure of size
        std::size_t size() const noexcept {
            return writer_.value_.load(std::memory_order_relaxed) -
                   reader_.value_.load(std::memory_order_relaxed);
        }

        // If queue is active this is NOT 100% accurate measure of empty
        bool empty() const noexcept {
            return writer_.value_.load(std::memory_order_relaxed) ==
                   reader_.value_.load(std::memory_order_relaxed);
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }
    };
} // namespace fastq

#endif // SPSC_QUEUE_H
