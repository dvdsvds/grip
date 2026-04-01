#include "grip/thread_pool.hpp"
#include <stdexcept>

using namespace grip;

ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
    for(size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this]{
            while(true) {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this] { return stop || !tasks.empty(); });
                if(stop && tasks.empty()) return;

                auto task = std::move(tasks.front());
                tasks.pop();
                lock.unlock();
                task();
            }
        });
    }
}

std::future<int> ThreadPool::submit(std::function<int()> task) {
    auto task_ptr = std::make_shared<std::packaged_task<int()>>(task);
    std::future<int> res = task_ptr->get_future();
    {
        std::unique_lock<std::mutex> lock(mtx);
        if(stop) throw std::runtime_error("ThreadPool stopped");

        tasks.push([task_ptr]() {
            (*task_ptr)();
        });
    }
    cv.notify_one();

    return res;
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        stop = true;
    }
    cv.notify_all();
    for(auto& worker : workers) {
        worker.join(); 
    }
}
