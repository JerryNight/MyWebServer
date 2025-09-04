#ifndef TIMER_H
#define TIMER_H

#include <functional>
#include <chrono>

using MSecond = std::chrono::milliseconds;
using Clock = std::chrono::steady_clock;
using TimeStamp = Clock::time_point;

struct TimerNode {
    int id;  // 定时器id
    TimeStamp expires; // 到期时间（时间戳）
    std::function<void()> cb;
    bool operator<(const TimerNode& rhs)const{
        return expires < rhs.expires;
    }
};

class Timer {
public:
    virtual ~Timer() = default;
    // 接口
    virtual void adjust(int id, int new_expires) = 0;
    virtual void add(int id, int timeout, const std::function<void()> & cb) = 0;
    virtual void doWork(int id) = 0;
    virtual void clear() = 0;
    virtual void tick() = 0;
    virtual void pop() = 0;
    virtual int GetNextTick() = 0;
};





#endif