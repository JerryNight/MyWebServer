#include "log.h"
#include <exception>


/**
 * 1. 设置日志级别
 * 2. 确定日志文件路径，文件后缀
 * 3. 初始化阻塞队列
 * 4. 打开日志文件，准备写入
 */
void Log::init(int level, const char* path, const char* suffix, int maxQueueCapacity){
    path_ = path ? path : "./";
    suffix_ = suffix ? suffix : ".log";
    is_open_ = true;
    level_ = level;
    
    if(maxQueueCapacity > 0) {
        is_async_ = true;
        if(!deque_) {
                std::unique_ptr<BlockQueue<std::string>> newDeque(new BlockQueue<std::string>);
                deque_ = std::move(newDeque);
                
                std::unique_ptr<std::thread> NewThread(new std::thread(FlushLogThread));
                write_thread_ = std::move(NewThread);
        }
    } else {
        is_async_ = false;
    }

    // 生成日志文件名
    time_t timer = time(nullptr);
    struct tm* sysTime = localtime(&timer);
    today_ = sysTime->tm_mday;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN, "%s/%04d_%02d_%02d%s",
             path_,
             sysTime->tm_year + 1900,
             sysTime->tm_mon + 1,
             sysTime->tm_mday,
             suffix_);
    
    // 打开日志文件
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // buffer_.RetrieveAll();
        if (fp_) {
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a");
        if (!fp_) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
    
    // 开启异步日志，初始化阻塞队列
    // if (maxQueueCapacity > 0) {
    //     is_async_ = true;
    //     if (!deque_) {
    //         deque_ = std::make_unique<BlockQueue<std::string>>(maxQueueCapacity);
    //         // 启动一个写日志的线程，负责将阻塞队列中的日志写到文件
    //         write_thread_ = std::make_unique<std::thread>([this](){
    //             std::string logStr;
    //             while (deque_ && deque_->pop(logStr)) {
    //                 std::lock_guard<std::mutex> lock(mtx_);
    //                 if (fp_){
    //                     fputs(logStr.c_str(), fp_);
    //                 }
    //             }
    //         });
    //         // write_thread_->detach(); // detach就无法join安全让线程退出了
    //     }
    // } else {
    //     is_async_ = false;
    // }
    

}

Log* Log::GetInstance(){
    static Log log;
    return &log;
}

void Log::FlushLogThread(){
    Log::GetInstance()->AsyncWrite_();
}

void Log::flush(){
    if (is_async_) {
        deque_->flush();
    }
    fflush(fp_);
}

int Log::GetLevel(){
    std::lock_guard<std::mutex> locker(mtx_);
    return level_;
}
void Log::SetLevel(int level){
    std::lock_guard<std::mutex> locker(mtx_);
    level_ = level;
}
bool Log::IsOpen(){
    std::lock_guard<std::mutex> locker(mtx_);
    return is_open_;
}

Log::Log(){
    deque_ = nullptr;
    write_thread_ = nullptr;
    fp_ = nullptr;
    today_ = 0;
    is_async_ = false;
    line_count_ = 0;
}
Log::~Log(){
    // 线程存在，且没有join/detach，异步线程进入不了这个条件
    if (write_thread_ && write_thread_->joinable()) {
        deque_->close();
        write_thread_->join(); // 线程退出
    }
    // 关闭文件
    if (fp_){
        std::lock_guard<std::mutex> locker(mtx_);
        flush();
        fclose(fp_);
        fp_ = nullptr;
    }
}
std::string Log::AppendLogLevelTitle_(int level){
    std::string title;
    switch(level) {
    case 0:
        // buffer_.Append("[debug]: ", 9);
        title = "[debug]: ";
        break;
    case 1:
        // buffer_.Append("[info] : ", 9);
        title = "[info] : ";
        break;
    case 2:
        // buffer_.Append("[warn] : ", 9);
        title = "[warn] : ";
        break;
    case 3:
        // buffer_.Append("[error]: ", 9);
        title = "[error]: ";
        break;
    default:
        // buffer_.Append("[info] : ", 9);
        title = "[info] : ";
        break;
    }
    return title;
}
void Log::AsyncWrite_(){
    std::string str = "";
    while (deque_->pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}
void Log::write(int level, const char* format,...){
    // 1.获取当前时间
    struct timeval now{}; // struct timeval{tv_sec秒数、tv_usec微妙数}
    gettimeofday(&now, nullptr); // 获取当前时间
    time_t tsec = now.tv_sec;    // 秒数
    struct tm t{};
    localtime_r(&tsec, &t);      // 把秒数分解为本地时间，保存在struct tm
    // 2.判断是否需要切换日志文件
    bool needNewFile = false;
    {
        std::lock_guard<std::mutex> locker(mtx_);
        if (today_ != t.tm_mday || (line_count_ && (line_count_ % MAX_LINES == 0))){
            needNewFile = true;
        }
    }
    if (needNewFile) {
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, sizeof(tail),"%04d_%02d_%02d",t.tm_year+1900,t.tm_mon+1,t.tm_mday);
        {
            std::lock_guard<std::mutex> locker(mtx_);
            flush();
            if (today_ != t.tm_mday) {
                snprintf(newFile, sizeof(newFile), "%s/%s%s",path_,tail,suffix_);
                today_ = t.tm_mday;
                line_count_ = 0;
            } else {
                snprintf(newFile, sizeof(newFile),"%s%s-%d%s",path_, tail,(line_count_ % MAX_LINES),suffix_);
            }
            if (fp_) fclose(fp_);
            fp_ = fopen(newFile, "a");
            assert(fp_ != nullptr);
        }
    }
    // 3.拼接日志内容
    std::string logMessage;
    {
        std::lock_guard<std::mutex> locker(mtx_);
        line_count_++;
        // 拼接时间前缀
        char timeBuff[128] = {0};
        int n = snprintf(timeBuff, sizeof(timeBuff),"%04d-%02d-%02d %02d:%02d:%02d.%06ld ",
                         t.tm_year + 1900,
                         t.tm_mon + 1,
                         t.tm_mday,
                         t.tm_hour,
                         t.tm_min,
                         t.tm_sec,
                         now.tv_usec);
        logMessage.append(timeBuff, n);
        // 拼接日志等级
        std::string logLevel = AppendLogLevelTitle_(level);
        logMessage.append(logLevel);
        // 拼接内容
        va_list vaList;
        va_start(vaList, format);
        int len = vsnprintf(nullptr,0,format, vaList);
        va_end(vaList);
        if (len > 0){
            std::string formatted(len, '\0');
            va_start(vaList, format);
            vsnprintf(&formatted[0], formatted.size() + 1, format, vaList);
            va_end(vaList);
            logMessage.append(formatted);
        }
        logMessage.append("\n"); // 日志换行
    }

    // 4.写入文件或队列
    if (is_async_ && deque_ && !deque_->full()){
        deque_->push_back(std::move(logMessage));
    } else {
        std::unique_lock<std::mutex> lock(mtx_);
        fputs(logMessage.c_str(), fp_); // fputs会把日志先写入文件缓冲区，
        fflush(fp_); // 
    }
}