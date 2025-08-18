#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <stdio.h>
#include <pthread.h>
#include <exception>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

template <typename T>
class block_queue {
public:
    block_queue(int capacity = 1000);
    ~block_queue();
    void clear();
    bool empty();
    bool full();
    bool front(T& value);
    bool back(T& value);
    bool push(const T& item);
    bool pop(T& item);
    bool pop(T& item, int timeout);
    int size();
    int capacity();


private:
    pthread_mutex_t mutex;
    pthread_cond_t cond_consumer;
    pthread_cond_t cond_producer;

    T* _array;
    int _capacity;
    int _size;
    int _front;
    int _back;
};


template <typename T>
block_queue<T>::block_queue(int capacity){
    if (capacity <= 0) throw std::execption();
    _array = new T[capacity];
    _capacity= capacity;
    _size = 0;
    _front = 0;
    _back = 0;
    pthread_mutex_init(&mutex);
    pthread_cond_init(&cond_consumer);
    pthread_cond_init(&cond_producer);
}

template <typename T>
block_queue<T>::~block_queue(){
    pthread_mutex_lock(&mutex);
    if (_array) {
        delete [] _array;
        _array = nullptr;
    }
    pthread_mutex_unlock(&mutex);
}

template <typename T>
void block_queue<T>::clear(){
    pthread_mutex_lock(&mutex);
    _size = 0;
    _front = 0;
    _back = 0;
    pthread_mutex_unlock(&mutex);
}

template <typename T>
bool block_queue<T>::empty(){
    pthread_mutex_lock(&mutex);
    bool ret = size == 0 ? true : false;
    pthread_mutex_unlock(&mutex);
    return ret;
}

template <typename T>
bool block_queue<T>::full(){
    pthread_mutex_lock(&mutex);
    bool ret = size == _capacity ? true : false;
    pthread_mutex_unlock(&mutex);
    return ret;
}

template <typename T>
bool block_queue<T>::front(T& value){
    if (empty()) return false;
    pthread_mutex_lock(&mutex);
    value = _array[front];
    pthread_mutex_unlock(&mutex);
    return true;
}

template <typename T>
bool block_queue<T>::back(T& value){
    if (empty()) return false;
    pthread_mutex_lock(&mutex);
    if (back == 0) {
        value = _array[_capacity];
    } else {
        value = _array[back - 1];
    }
    pthread_mutex_unlock(&mutex);
    return true;
}

template <typename T>
bool block_queue<T>::push(const T& item){
    pthread_mutex_lock(&mutex);
    while (_size == _capacity) {
        pthread_cond_wait(&cond_producer, &mutex);
    }
    _array[back] = item;
    back = back == _capacity ? 0 : back + 1;
    pthread_cond_broadcast(&cond_consumer);
    pthread_mutex_unlock(&mutex);
    return true;
}

template <typename T>
bool block_queue<T>::pop(T& item){
    pthread_mutex_lock(&mutex);
    while (_size == 0) {
        pthread_cond_wait(&cond_consumer, &mutex);
    }
    item = _array[front];
    front = front == _capacity ? 0 : front + 1;
    pthread_cond_broadcast(&cond_producer);
    pthread_mutex_unlock(&mutex);
    return true;
}

// 超时处理
template <typename T>
bool block_queue<T>::pop(T& item, int timeout){
    // 设置超时时间1s
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 1;
    // 主逻辑
    pthread_mutex_lock(&mutex);
    while (_size == 0) {
        if (pthread_cond_timewait(&cond_consumer, &mutex, &ts) == ETIMEDOUT) {
            pthread_mutex_unlock(&mutex);
            return false;
        }
    }
    item = _array[front];
    front = front == _capacity ? 0 : front + 1;
    pthread_cond_broadcast(&cond_producer);
    pthread_mutex_unlock(&mutex);
    return true;
}

template <typename T>
int block_queue<T>::size(){
    return _size;
}

template <typename T>
int block_queue<T>::capacity(){
    return _capacity;
}



#endif