#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <memory>
#include <type_traits>
#include <utility>

namespace fastq {
    template <typename T>
    concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

    template <TriviallyCopyable T, typename Allocator = std::allocator<T>>
    class SPSC {
      public:
        explicit SPSC(std::size_t capacity)
            : capacity_(capacity), size_(0), alloc_(),
              buffer_(std::allocator_traits<Allocator>::allocate(alloc_, capacity_)) {}

        SPSC(const SPSC&) = delete;

        SPSC& operator=(const SPSC&) = delete;

        SPSC(SPSC&& other)
            : alloc_(std::move(other.alloc_)), buffer_(std::exchange(other.buffer_, nullptr)),
              capacity_(std::exchange(other.capacity_, 0)), size_(std::exchange(other.size_, 0)) {}

        SPSC& operator=(SPSC&& other) noexcept {
            if (this != &other) {
                if (buffer_) {
                    std::allocator_traits<Allocator>::deallocate(alloc_, buffer_, capacity_);
                }

                alloc_ = std::move(other.alloc_);
                buffer_ = std::exchange(other.buffer_, nullptr);
                capacity_ = std::exchange(other.capacity_, 0);
                size_ = std::exchange(other.size_, 0);
            }
            return *this;
        }

        ~SPSC() {
            if (buffer_) {
                std::allocator_traits<Allocator>::deallocate(alloc_, buffer_, capacity_);
            }
        }

        bool push() {
            if (size_ == capacity_) {
                return false;
            }
        }

        bool pop() {
            if (size_ == 0) {
                return false;
            }
        }

        std::size_t size() const {
            return size_;
        }

        std::size_t capacity() const {
            return capacity_;
        }

        bool empty() const {
            return size_ == 0;
        }

      private:
        T* buffer_;
        Allocator alloc_;
        std::size_t capacity_;
        std::size_t size_;
    };
} // namespace fastq

#endif // SPSC_QUEUE_H
