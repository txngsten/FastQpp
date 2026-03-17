#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <memory>

namespace fastq {
    template <typename T, typename Allocator = std::allocator<T>>
    class SPSC {
      public:
        explicit SPSC(std::size_t capacity) : capacity_(capacity), size_(0), alloc_() {
            buffer_ = std::allocator_traits<Allocator>::allocate(alloc_, capacity_);
        }

        ~SPSC() {
            std::allocator_traits<Allocator>::deallocate(alloc_, buffer_, capacity_);
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
            return capacity_ - size_;
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
