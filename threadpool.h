#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"
#include "sql_connection_pool.h"

using std::list;

template <typename T>
class threadpool{
public:
    threadpool(int actor_model, sql_connection_pool* sqlConnPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    //
    bool append(T* request, int state);
    bool append_p(T* request);

private:
    // 工作线程的worker
    static void* worker(void* arg);
    void run();

private:
    int _thread_number; // 线程数
    int _max_request;   // 最大连接请求
    pthread_t* _threads; // 线程池数组
    list<T*> _workqueue; // 请求队列
    locker _queuelocker; // 阻塞队列的锁
    sem _queuesem;       // 阻塞队列信号量
    sql_connection_pool* _sql_pool; // 数据库连接池
    int _actor_mode; // 响应模式
};

template <typename T>
threadpool<T>::threadpool(int actor_model, sql_connection_pool* sqlConnPool, int thread_number, int max_request){
    if (thread_number <= 0 || max_request <= 0) throw std::execption();
    _thread_number = thread_number;
    _max_request = max_request;
    _sql_pool = sqlConnPool;
    // 创建线程池
    _threads = new pthread_t[thread_number];
    for (int i = 0; i < thread_number; i++) {
        pthread_create(&_thread[i], NULL, worker, this);
        pthread_detach(_threads[i]);
    }
}

template <typename T>
threadpool<T>::~threadpool(){
    delete [] _threads;
}

template <typename T>
bool threadpool<T>::append(T* request, int state){
    _queuelocker.lock();
    if (_workqueue.size() == _max_request) {
        _queuelocker.unlock();
        return false;
    }
    request->_state = state;
    _workqueue.push_back(request);
    _queuelocker.unlock();
    _queuesem.post();
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T* request){
    _queuelocker.lock();
    if (_workqueue.size() == _max_request) {
        _queuelocker.unlock();
        return false;
    }
    _workqueue.push_back(request);
    _queuelocker.unlock();
    _queuesem.post();
    return true;
}

template <typename T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool = (threadpool*) arg;
    pool.run();
    return pool;
}

template <typename T>
void threadpool<T>::run(){
    for (;;) {
        _queuesem.wait();
        _queuelocker.lock();
        T* request = _workqueue.front();
        _workqueue.pop_front();
        _queuelocker.unlock();
        if (request == NULL) continue;
        if (_actor_mode == 1) { // 0-proactor  1-reactor：epoll收到就绪事件后，由应用程序主动读socket缓冲区的数据，再处理结果。
            if (request->_state == 0) { // 0读 1写
                if (request.read_once()) {
                    request->improv = 1;
                    sql_connection_RAII sqlcon(&request->mysql, sqlConnPool);
                    request->process();
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            } else { // 写
                if (request->write()){
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        } else {  // proactor: 由操作系统完成IO操作（读socket缓冲区的数据），应用程序只处理结果
            sql_connection_RAII sqlcon(&request->mysql, sqlConnPool);
            request->process();
        }
    }
}

#endif