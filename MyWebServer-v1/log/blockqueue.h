#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <deque>
#include <mutex>
#include <condition_variable>
#include <cassert>

template <typename T>
class BlockQueue {
public:
    explicit BlockQueue(size_t capacity = 1024);
    ~BlockQueue();

    bool empty();
    bool full();
    void close();
    void clear();
    size_t size();
    size_t capacity();
    T front();
    T back();
    void push_front(const T& item);
    void push_back(const T& item);
    void push_back(T&& item);
    bool pop(T& item);
    bool pop(T& item, int timeout);
    void flush();

private:
    std::deque<T> deq_;
    std::mutex mtx_;
    size_t capacity_;
    bool is_close_; // 队列关闭
    std::condition_variable cond_consumer_;
    std::condition_variable cond_producer_;
};

template <typename T>
BlockQueue<T>::BlockQueue(size_t capacity):capacity_(capacity){
    assert(capacity > 0);
    is_close_ = false;
}
template <typename T>
BlockQueue<T>::~BlockQueue(){
    close();
}
template <typename T>
bool BlockQueue<T>::empty(){
    std::lock_guard<std::mutex> lock(mtx_);
    return deq_.empty();
}
template <typename T>
bool BlockQueue<T>::full(){
    std::lock_guard<std::mutex> lock(mtx_);
    return deq_.size() >= capacity_;
}
/**
 * 关闭阻塞队列：
 */
template <typename T>
void BlockQueue<T>::close(){
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // deq_.clear();  // 清空消息队列
        is_close_ = true; // 关闭
    }
    cond_consumer_.notify_all(); // 唤醒所有阻塞的消费线程
    cond_producer_.notify_all(); // 唤醒所有阻塞的生产线程
}
template <typename T>
void BlockQueue<T>::clear(){
    std::lock_guard<std::mutex> lock(mtx_);
    deq_.clear();
}
template <typename T>
size_t BlockQueue<T>::size(){
    std::lock_guard<std::mutex> lock(mtx_);
    return deq_.size();
}
template <typename T>
size_t BlockQueue<T>::capacity(){
    std::lock_guard<std::mutex> lock(mtx_);
    return capacity_;
}
template <typename T>
T BlockQueue<T>::front(){
    std::lock_guard<std::mutex> lock(mtx_);
    return deq_.front();
}
template <typename T>
T BlockQueue<T>::back(){
    std::lock_guard<std::mutex> lock(mtx_);
    return deq_.back();
}
template <typename T>
void BlockQueue<T>::push_front(const T& item){
    std::unique_lock<std::mutex> lock(mtx_);
    cond_producer_.wait(lock,[this]{return deq_.size() == capacity_ || is_close_;});
    if (is_close_) return;
    deq_.push_front(item);
    cond_consumer_.notify_one();
}
template <typename T>
void BlockQueue<T>::push_back(const T& item){
    std::unique_lock<std::mutex> lock(mtx_);
    //cond_producer_.wait(lock, [this]{return deq_.size() == capacity_ || is_close_;});
    while(deq_.size() >= capacity_) {
        cond_producer_.wait(lock);
        if (is_close_) return;
    }
    deq_.push_back(item);
    cond_consumer_.notify_one();
}
template <typename T>
void BlockQueue<T>::push_back(T&& item){
    std::unique_lock<std::mutex> lock(mtx_);
    cond_producer_.wait(lock, [this]{return deq_.size() < capacity_ || is_close_;});
    // while(deq_.size() >= capacity_) {
    //     cond_producer_.wait(lock);
    //     if (is_close_) return;
    // }
    deq_.push_back(std::move(item));
    cond_consumer_.notify_one();
}
template <typename T>
bool BlockQueue<T>::pop(T& item){
    std::unique_lock<std::mutex> lock(mtx_);
    cond_consumer_.wait(lock, [this]{return deq_.size() > 0 || is_close_;});
    // while(deq_.empty()){
    //     cond_consumer_.wait(lock);
    //     if(is_close_){
    //         return false;
    //     }
    // }
    if (is_close_) return false;
    item = deq_.front();
    deq_.pop_front();
    cond_producer_.notify_one();
    return true;
}
template <typename T>
bool BlockQueue<T>::pop(T& item, int timeout){
    std::unique_lock<std::mutex> lock(mtx_);
    while (deq_.size() == 0 && !is_close_) {
        if (cond_consumer_.wait_for(lock, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        }
    }
    if (is_close_ && deq_.empty()) return false; // 队列关闭且为空
    item = deq_.front();
    deq_.pop_front();
    cond_producer_.notify_one();
    return true;
}
template <typename T>
void BlockQueue<T>::flush(){
    cond_consumer_.notify_one();
}



#endif