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
            : buffer_(nullptr), alloc_(), mask_(0), capacity_(capacity) {
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

        [[nodiscard]] bool push(T data) noexcept {
            auto writer = writer_.value.load(std::memory_order_relaxed);

            if (writer - cachedReader_ == capacity_) {
                cachedReader_ = reader_.value.load(std::memory_order_acquire);

                if (writer - cachedReader_ == capacity_) {
                    return false;
                }
            }

            buffer_[writer & mask_] = data;
            writer_.value.store(writer + 1, std::memory_order_release);

            return true;
        }

        [[nodiscard]] bool pop(T& data) noexcept {
            auto reader = reader_.value.load(std::memory_order_relaxed);

            if (cachedWriter_ == reader) {
                cachedWriter_ = writer_.value.load(std::memory_order_acquire);

                if (cachedWriter_ == reader) {
                    return false;
                }
            }

            data = buffer_[reader & mask_];
            reader_.value.store(reader + 1, std::memory_order_release);

            return true;
        }

        std::size_t size() const noexcept {
            return writer_.value.load(std::memory_order_relaxed) -
                   reader_.value.load(std::memory_order_relaxed);
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }

        bool empty() const noexcept {
            return writer_.value.load(std::memory_order_relaxed) ==
                   reader_.value.load(std::memory_order_relaxed);
        }

      private:
        T* buffer_;
        Allocator alloc_;

        struct alignas(std::hardware_destructive_interference_size) Writer {
            std::atomic<std::size_t> value{0};
        };
        struct alignas(std::hardware_destructive_interference_size) Reader {
            std::atomic<std::size_t> value{0};
        };

        Writer writer_;
        Reader reader_;

        std::size_t cachedReader_{0};
        std::size_t cachedWriter_{0};

        std::size_t mask_;
        std::size_t capacity_;
    };
} // namespace fastq

#endif // SPSC_QUEUE_H
