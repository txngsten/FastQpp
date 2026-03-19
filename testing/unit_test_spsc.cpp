#include "../spsc_queue.hpp"
#include <gtest/gtest.h>
#include <type_traits>

extern "C" {
    void __ubsan_on_report() {
        FAIL() << "Encountered an undefined behavior sanitizer error";
    }
    void __asan_on_error() {
        FAIL() << "Encountered an address sanitizer error";
    }
    void __tsan_on_report() {
        FAIL() << "Encountered a thread sanitizer error";
    }
}

template <typename FastQT>
class FastQTestingBase : public testing::Test {
  public:
    using FastQType = FastQT;
    using value_type = typename FastQType::value_type;
};
