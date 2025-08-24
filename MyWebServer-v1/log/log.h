#ifndef LOG_H_
#define LOG_H_

#include <string>
#include <thread>
#include <sys/stat.h> // mkdir
#include <cassert>
#include <sys/time.h>
#include <stdarg.h>
// #include <memory> // make_unique
#include "../buffer/buffer.h"
#include "blockqueue.h"


/**
 * 支持异步写的日志系统。
 * 主要功能：1.单例模式，全局只有一个日志对象。2.异步写，通过blockqueue把日志写入后台线程
 * 3.控制打印日志级别：DEBUG/INFO/WARN/ERROR
 * 4.按天、按行数分割日志文件
 */

class Log {
public:
    void init(int level = 1, const char* path = "./log", const char* suffix = ".log", int maxQueueCapacity = 1024);
    static Log* GetInstance();
    static void FlushLogThread();

    void write(int level, const char* format,...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen();

private:
    Log();
    virtual ~Log();
    std::string AppendLogLevelTitle_(int level);
    void AsyncWrite_();
private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 5000;

    const char* path_;
    const char* suffix_;

    int max_lines_;
    int line_count_;
    int today_;
    bool is_open_;
    //Buffer buffer_; // 缓冲区
    int level_;
    bool is_async_;

    FILE* fp_;  // 文件指针
    std::unique_ptr<BlockQueue<std::string>> deque_; // 异步队列
    std::unique_ptr<std::thread> write_thread_;      // 写日志线程
    std::mutex mtx_;

};

#define LOG_BASE(level, format, ...) \
    do { \
        Log* log = Log::GetInstance(); \
        if (log->IsOpen() && log->GetLevel() <= level) { \
            log->write(level, format, ##__VA_ARGS__); \
            log->flush(); \
        } \
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while (0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while (0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while (0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while (0);

#endif