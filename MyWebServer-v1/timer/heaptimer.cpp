#include "heaptimer.h"
#include <cassert>



int HeapTimer::GetNextTick(){
    tick();
    int res = -1;
    if (!heap.empty()) {
        res = std::chrono::duration_cast<MSecond>(heap.front().expires - Clock::now()).count();
        res = res < 0 ? 0 : res;
    }
    return res;
}

void HeapTimer::clear(){
    ref.clear();
    heap.clear();
}

void HeapTimer::doWork(int id){
    // 删除该id，执行回调函数
    if (heap.empty() || ref.count(id) == 0) return;
    size_t index = ref[id];
    TimerNode node = heap[index];
    node.cb();
    del(id);
}

void HeapTimer::tick(){
    if (heap.empty()) return;
    while (!heap.empty()) {
        TimerNode node = heap.front();
        if (std::chrono::duration_cast<MSecond>(node.expires - Clock::now()).count() > 0){
            break;
        }
        pop();
        node.cb();
    }
}
void HeapTimer::pop(){
    assert(!heap.empty());
    del(0);
}

void HeapTimer::add(int id, int timeout, const std::function<void()> & cb){
    assert(id > 0);
    // 已有该timer,调整timeout
    if (ref.count(id) > 0) {
        size_t index = ref[id];
        heap[index].expires = Clock::now() + MSecond(timeout);
        heap[index].cb = cb;
        flush(index, heap.size());
    } else {
        // 添加一个TimerNode
        size_t index = heap.size();
        ref[id] = index;
        heap.push_back({id,Clock::now() + MSecond(timeout), cb});
        heapInsert(index);
    }
}

void HeapTimer::adjust(int id, int new_expires){
    assert(!heap.empty() && ref.count(id) > 0);
    size_t index = ref[id];
    heap[index].expires = Clock::now() + MSecond(new_expires);
    flush(index, heap.size());
}


// 删除指定元素
void HeapTimer::del(size_t index){
    assert(!heap.empty() && index < heap.size());
    size_t size = heap.size() - 1;
    int id = heap[index].id;
    swap(index, size);
    ref.erase(id);
    heap.pop_back();
    flush(index, size);
}
void HeapTimer::heapInsert(size_t index){
    // size_t dad = (index - 1) / 2;
    // while (heap[index] < heap[dad]) {
    //     swap(index, dad);
    //     index = dad;
    //     dad = (index - 1) / 2;
    // }
    while (index > 0) {
        size_t dad = (index - 1) / 2;
        if (!(heap[index] < heap[dad])) break;
        swap(index, dad);
        index = dad;
    }
}
bool HeapTimer::heapify(size_t index, size_t size){
    size_t i = index;
    size_t left = (index * 2) + 1;
    while (left < size) {
        size_t min = left + 1 < size && heap[left + 1] < heap[left] ? left + 1 : left;
        if (heap[index] < heap[min]) break;
        swap(index, min);
        index = min;
        left = (index * 2) + 1; 
    }
    return i < index;
}
void HeapTimer::swap(size_t i, size_t j){
    assert(i >= 0 && i < heap.size());
    assert(j >= 0 && j < heap.size());
    std::swap(heap[i], heap[j]);
    ref[heap[i].id] = i;
    ref[heap[j].id] = j;
}
void HeapTimer::flush(size_t index, size_t size){
    if (!heapify(index, size))
        heapInsert(index);
}