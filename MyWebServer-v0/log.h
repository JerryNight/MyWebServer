#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include "block_queue.h"
#include "locker.h"
using std::string;

class Log {
public:
    static Log& get_instance(){
        static Log instance;
        return instance;
    }
    // 异步写日志
    static void *flush_log_thread(void *args){
        Log::get_instance().async_write_log();
    }
    // 初始化
    bool init(const char* filename, int close_log, int log_buf_size = 8192, int split_lines = 50000000, int max_queue_size = 0);
    // 写日志
    void write_log(int level, const char *format, ...);
    // 刷新到磁盘
    void flush(void);
private:
    Log(){
        _count = 0;
        _is_async = false; // 默认同步写
    }
    ~Log(){
        if (_fp) fclose(_fp);
    }
    // 异步写日志
    void* async_write_log(){
        string single_log;
        while (_log_queue->pop(single_log)){
            lock.lock();
            fputs(single_log.c_str(), _fp);
            lock.unlock();
        }
    }

private:
    char dir_name[128]; // 路径名
    char log_name[128]; // log文件名
    int _split_lines;   // 日志最大行数
    int _log_buf_size;  // 日志缓冲区大小
    long long _count;   // 日志行数记录
    int _today;         // 按天分类，记录当天是哪一天
    FILE* _fp;          // 打开log的文件指针
    char* _buf; // 日志缓冲区
    block_queue<string>* _log_queue; // 阻塞队列
    bool _is_async;     // 是否同步标志位
    locker lock;
    int _close_log;     // 关闭日志

};

#define LOG_DEBUG(format, ...) if(closeLog == 0){Log::get_instance().write_log(0, format, ##__VA_ARGS__); Log::get_instance().flush();}
#define LOG_INFO(format, ...) if(closeLog == 0){Log::get_instance().write_log(1, format, ##__VA_ARGS__); Log::get_instance().flush();}
#define LOG_WARN(format, ...) if(closeLog == 0){Log::get_instance().write_log(2, format, ##__VA_ARGS__); Log::get_instance().flush();}
#define LOG_ERROR(format, ...) if(closeLog == 0){Log::get_instance().write_log(3, format, ##__VA_ARGS__); Log::get_instance().flush();}



#endif