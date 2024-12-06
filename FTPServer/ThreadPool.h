#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <list>

class ThreadPool {
private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::list<std::function<void()> > m_queue;
    bool m_end;
    std::vector<std::thread> m_threads;

    void run();

public:
    explicit ThreadPool(size_t nrThreads);

    ~ThreadPool();

    void close();

    void enqueue(std::function<void()> func);
};