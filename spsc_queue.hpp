#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace fastq {
    template <typename T, typename Allocator = std::allocator<T>>
        requires std::is_trivially_copyable_v<T>
    class SPSC {
      public:
        explicit SPSC(std::size_t capacity)
            : buffer_(nullptr), alloc_(), writer_(0), reader_(0), mask_(0), capacity_(capacity) {
            if (capacity_ == 0 || (capacity_ & (capacity_ - 1)) != 0) {
                throw std::invalid_argument("Capacity must be non-zero power of 2");
            }

            mask_ = capacity_ - 1;
            buffer_ = std::allocator_traits<Allocator>::allocate(alloc_, capacity_);
        }

        SPSC(const SPSC&) = delete;
        SPSC& operator=(const SPSC&) = delete;
        SPSC(SPSC&& other) = delete;
        SPSC& operator=(SPSC&& other) = delete;

        ~SPSC() {
            if (buffer_) {
                std::allocator_traits<Allocator>::deallocate(alloc_, buffer_, capacity_);
            }
        }

        bool push(T data) noexcept {
            auto writer = writer_.load(std::memory_order_relaxed);
            auto reader = reader_.load(std::memory_order_acquire);

            if (writer - reader == capacity_) {
                return false;
            }

            buffer_[writer & mask_] = data;
            writer_.store(writer + 1, std::memory_order_release);

            return true;
        }

        bool pop(T& data) noexcept {
            auto reader = reader_.load(std::memory_order_relaxed);
            auto writer = writer_.load(std::memory_order_acquire);

            if (writer == reader) {
                return false;
            }

            data = buffer_[reader & mask_];
            reader_.store(reader + 1, std::memory_order_release);

            return true;
        }

        std::size_t size() const noexcept {
            return writer_.load(std::memory_order_acquire) -
                   reader_.load(std::memory_order_acquire);
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }

        bool empty() const noexcept {
            return writer_.load(std::memory_order_acquire) ==
                   reader_.load(std::memory_order_acquire);
        }

      private:
        T* buffer_;
        Allocator alloc_;

        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> writer_;
        alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> reader_;

        std::size_t mask_;
        std::size_t capacity_;
    };
} // namespace fastq

#endif // SPSC_QUEUE_H
