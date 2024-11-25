#ifndef THREADSAFEPRIORITYQUEUE_HPP
#define THREADSAFEPRIORITYQUEUE_HPP
#include <queue>
#include <mutex>
#include <map>
#include <unordered_map>
#include <condition_variable>

template<typename T>
class ThreadSafePriorityQueue
{
private:
    std::priority_queue<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;

public:
    void push(T value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(value);
        m_condition.notify_one();
    }

    void pop()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.pop();
    }

    // 返回队列顶部元素但不移除它
    T top() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            throw std::logic_error("Queue is empty.");
        }
        return m_queue.top();
    }

    // 返回队列是否为空
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    // 等待直到队列非空，然后返回队列顶部元素
    T wait_and_pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [&] { return !m_queue.empty(); });
        T value = m_queue.top();
        m_queue.pop();
        return value;
    }
    // 复制构造函数
    ThreadSafePriorityQueue(const ThreadSafePriorityQueue& other) : m_queue(other.m_queue)
    {
    }
    ThreadSafePriorityQueue()
    {
    }
};
#endif