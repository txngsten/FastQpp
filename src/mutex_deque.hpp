#ifndef MUTEX_DEQUE_H
#define MUTEX_DEQUE_H

#include <mutex>
#include <utility>
#include <deque>

template <typename T>
class MutexDeque {
public:
    MutexDeque() = default;

    void write(T data) {
        std::lock_guard<std::mutex> lock(m);
        dq.push_back(data);
    }

    bool read(T& data) {
        std::lock_guard<std::mutex> lock(m);

        if (dq.empty()) {
            return false;
        }

        data = std::move(dq.front());
        dq.pop_front();

        return true;
    }

private:
    std::deque<T> dq;
    std::mutex m;

};

#endif // MUTEX_DEQUE_H
