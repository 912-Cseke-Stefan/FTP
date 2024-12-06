#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t nrThreads)
    :m_end(false)
{
    m_threads.reserve(nrThreads);
    for (size_t i = 0; i < nrThreads; ++i) {
        m_threads.emplace_back([this]() { this->run(); });
    }
}

ThreadPool::~ThreadPool()
{
    close();
    for (std::thread& t : m_threads) {
        t.join();
    }
}

void ThreadPool::close() {
    std::unique_lock<std::mutex> lck(m_mutex);
    m_end = true;
    m_cond.notify_all();
}

void ThreadPool::enqueue(std::function<void()> func) {
    std::unique_lock<std::mutex> lck(m_mutex);
    m_queue.push_back(std::move(func));
    m_cond.notify_one();
}

void ThreadPool::run() {
    while (true) {
        std::function<void()> toExec;
        {
            std::unique_lock<std::mutex> lck(m_mutex);
            while (m_queue.empty() && !m_end) {
                m_cond.wait(lck);
            }
            if (m_queue.empty()) {
                return;
            }
            toExec = std::move(m_queue.front());
            m_queue.pop_front();
        }
        toExec();
    }
}
