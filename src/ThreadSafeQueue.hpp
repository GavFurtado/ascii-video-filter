#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <utility>

namespace AsciiVideoFilter {

template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(std::move(item));
        m_cv.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_queue.empty() || m_stop; });
        if (m_stop && m_queue.empty()) {
            return T(); // Signals shutdown or no more items
        }
        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    void stop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_stop = true;
        m_cv.notify_all(); // Wake up all waiting threads
    }

    bool empty() const {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    std::queue<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
};

} // namespace AsciiVideoFilter
