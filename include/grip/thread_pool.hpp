#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
namespace grip {
    class ThreadPool {
        private:
            std::vector<std::thread> workers;
            std::queue<std::function<void()>> tasks;
            std::mutex mtx;
            std::condition_variable cv;
            bool stop;
        public:
            ThreadPool(size_t num_threads);
            std::future<int> submit(std::function<int()> task);
            ~ThreadPool();
    };
}
