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

bool Log::init(const char* filename, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    // 设置了异步写
    if (max_queue_size > 0) {
        _is_async = true;
        _log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid,NULL, flush_log_thread, NULL);
    }
    _close_log = close_log;
    // 日志缓冲区
    _log_buf_size = log_buf_size;
    _buf = new char[_log_buf_size];
    bzero(_buf, _log_buf_size);
    _split_lines = split_lines;
    // 设置当前时间
    time_t t = time(NULL);
    struct tm my_tm;
    localtime_r(&t, &my_tm);
    // 设置文件名
    const char* p = strrchr(filename, '/'); // 查找最后一个'/'的位置
    char log_full_name[256] = {0};  // log日志的文件名
    if (p == NULL) {
        // 如果文件名中，没有路径分隔符'/'，说明传入的filename是完整的文件名
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, filename);
    } else {
        strcpy(log_name, p + 1); // 取出传入的filename
        strncpy(dir_name, filename, p - filename + 1); // 取出路径名
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }
    _today = my_tm.tm_mday;
    // 打开日志文件
    _fp = fopen(log_full_name, "a");
    if (!_fp) return false;
    return true;
}

void Log::write_log(int level, const char *format, ...){
    // 获取当前时间
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm my_tm;
    localtime_r(&t, &my_tm); 
    // 日志记录的开头
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    // 写入日志
    lock.lock();
    _count++;
    // 如果不是当天，或者文件满了
    if (_today != my_tm.tm_mday || _count % _split_lines == 0) {
        // 关掉前一天的日志文件
        fflush(_fp);
        fclose(_fp);
        // 日志记录的：年_月_日
        char tail[16] = {0};
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        // 新的日志文件名称
        char new_log[256] = {0};
        if (_today != my_tm.tm_mday) {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            _today = my_tm.tm_mday;
            _count = 0;
        } else {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, _count / _split_lines);
        }
        // 打开新文件
        _fp = fopen(new_log, "a");
    }
    lock.unlock();

    va_list valst;
    va_start(valst, format);
    string log_str;
    lock.lock();
    // 把时间写进buf
    int n = snprintf(_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                 my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                 my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    // 把用户自定义的内容format 写入buf
    int m = vsnprintf(_buf + n, _log_buf_size - n - 1, format, valst);
    _buf[n + m] = '\n';  // 换行
    _buf[n + m + 1] = '\0';  // 字符串结束符
    log_str = _buf;  // 存入 string 对象
    lock.unlock();

    // 如果是异步写入，且log_queue不满
    if (_is_async && !_log_queue->full()){
        _log_queue->push(log_str);
    } else {
        // 同步写
        lock.lock();
        fputs(log_str.c_str(), _fp);
        lock.unlock();
    }
    // 清理可变参数列表
    va_end(valst);
}

void Log::flush(void){
    // 将fd文件缓冲区内的数据，提交给操作系统，写入磁盘
    lock.lock();
    fflush(_fp);
    lock.unlock();
}

#define LOG_DEBUG(format, ...) if(closeLog == 0){Log::get_instance().write_log(0, format, ##__VA_ARGS__); Log::get_instance().flush();}
#define LOG_INFO(format, ...) if(closeLog == 0){Log::get_instance().write_log(1, format, ##__VA_ARGS__); Log::get_instance().flush();}
#define LOG_WARN(format, ...) if(closeLog == 0){Log::get_instance().write_log(2, format, ##__VA_ARGS__); Log::get_instance().flush();}
#define LOG_ERROR(format, ...) if(closeLog == 0){Log::get_instance().write_log(3, format, ##__VA_ARGS__); Log::get_instance().flush();}



#endif