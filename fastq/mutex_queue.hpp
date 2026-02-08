#ifndef MUTEX_QUEUE_H
#define MUTEX_QUEUE_H

#include <mutex>
#include <utility>
#include <queue>

template <typename  T>
class MutexQ {
public:
    MutexQ() = default;

    void write(T data) {
        std::lock_guard<std::mutex> lock(m);
        q.push(std::move(data));
    }

    bool read(T& data) {
        std::lock_guard<std::mutex> lock(m);

        if (q.empty()) {
            return false;
        }

        data = std::move(q.front());
        q.pop();

        return true;
    }


private:
    std::queue<T> q;
    std::mutex m;
};

#endif // MUTEX_QUEUE_H