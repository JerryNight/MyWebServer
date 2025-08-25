#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <cassert>
#include <vector>
#include <thread>

class ThreadPool {
public:
    // 创建线程池
    explicit ThreadPool(size_t count = 8):is_closed(false){
        assert(count > 0);
        for (size_t i = 0; i < count; i++) {
            workers.emplace_back([this]{
                while (true) {
                    std::function<void()> task;
                    {   // 临界区
                        std::unique_lock<std::mutex> lock(mtx);
                        cond.wait(lock, [this]{return is_closed || !tasks.empty();});
                        // 如果线程关闭，且任务队列为空
                        if (is_closed && tasks.empty()){
                            return;
                        } 
                        task = std::move(tasks.front());
                        tasks.pop();

                    }
                    task();
                }
            });
        }
    }

    // 析构
    ~ThreadPool(){
        {
            std::unique_lock<std::mutex> lock(mtx);
            is_closed = true;
        }
        // 唤醒所有线程
        cond.notify_all();
        // 等待退出
        for (auto & worker : workers){
            if (worker.joinable()){
                worker.join();
            }
        }
    }

    // 向任务队列添加任务：fire and forget
    // 模板T&& 为万能引用，既能处理左值，也能处理右值
    // std::forward<T> 
    template <typename T>
    void AddTask(T&& task) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            tasks.emplace(std::forward<T>(task));
        }
        cond.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::mutex mtx;
    std::condition_variable cond;
    std::queue<std::function<void()>> tasks;
    bool is_closed;

};






#endif