#ifndef HEAPTIMER_H
#define HEAPTIMER_H

#include "timer.h"
#include <vector>
#include <unordered_map>


class HeapTimer : public Timer {
public:
    // 接口
    virtual void adjust(int id, int new_expires);
    virtual void add(int id, int timeout, const std::function<void()> & cb);
    virtual void doWork(int id);
    virtual void clear();
    virtual void tick();
    virtual void pop();
    virtual int GetNextTick();

private:
    void del(size_t index);
    void heapInsert(size_t index);
    void heapify(size_t index, size_t size);
    void swap(size_t i, size_t j);
    void flush(size_t index, size_t size);
private:
    std::vector<TimerNode> heap;
    std::unordered_map<int, size_t> ref; // <id, index>
};




#endif